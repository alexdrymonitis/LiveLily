#ifndef CLICKTRACK_H
#define CLICKTRACK_H

#include "ofMain.h"
#include "clock.h"
#include <atomic>
#include <cmath>
#include <mutex>
#include <vector>
#include <algorithm>

// a click track that clicks on the beats of \beatat (the pulses of the beat visualization), see \clicktrack
// each click is a short triangle wave, at the tuning frequency (440Hz by default), and an octave higher on the first beat of a bar
// the sequencer thread triggers the clicks with the time of their beats, and the audio thread plays them (see ofApp::audioOut())
// the clicks are sample-accurate: the audio thread maps the time of every beat to a sample of its output
// from the time its buffers are processed, and starts the click at that sample, a fixed latency later (see LATENCY_BUFFERS)
class ClickTrack
{
	public:
		ClickTrack()
		{
			pending.reserve(64);
			scheduled.reserve(64);
		}

		void setOn(bool state)
		{
			on = state;
			if (!state) {
				std::lock_guard<std::mutex> lock(pendingMutex);
				pending.clear();
				clearScheduled = true;
			}
		}

		bool isOn()
		{
			return on;
		}

		void setTune(float freq)
		{
			tune = freq;
		}

		float getTune()
		{
			return tune;
		}

		// the peak amplitude of the clicks, from 0 to 1
		void setVolume(float vol)
		{
			volume = vol;
		}

		float getVolume()
		{
			return volume;
		}

		// called by the sequencer on every beat, with the time of the beat in microseconds (Clock::micros())
		// accent is true for the first beat of a bar
		void trigger(bool accent, uint64_t timeUs)
		{
			if (!on) return;
			std::lock_guard<std::mutex> lock(pendingMutex);
			pending.push_back({(int64_t)timeUs, accent});
		}

		// called by the audio thread, adds the clicks to all the channels of the buffer
		void process(ofSoundBuffer& buffer)
		{
			double sampleRate = buffer.getSampleRate();
			int numChannels = (int)buffer.getNumChannels();
			int numFrames = (int)buffer.getNumFrames();
			if (sampleRate <= 0 || numFrames <= 0) return;
			// the time of the first sample of the output, from the time this buffer is processed and the samples processed before it
			// the callbacks come a bit early or late, so the time is smoothed
			// and set again if it is far off, e.g. when the stream starts again with other settings
			int64_t now = (int64_t)Clock::micros();
			double streamStart = (double)now - ((double)framesDone * 1000000.0 / sampleRate);
			if (!streamStartSet || std::fabs(streamStart - streamStartUs) > RESYNC_US || sampleRate != lastSampleRate) {
				streamStartUs = streamStart;
				streamStartSet = true;
				lastSampleRate = sampleRate;
			}
			else {
				streamStartUs += (streamStart - streamStartUs) * SMOOTHING;
			}
			if (clearScheduled.exchange(false)) {
				scheduled.clear();
				clicking = false;
			}
			// the beats the sequencer has triggered, at their samples, a fixed latency later, so that they are still to come
			// (if the lock is busy, they are scheduled at the next buffer, which is within the latency)
			if (pendingMutex.try_lock()) {
				int64_t latencyFrames = (int64_t)numFrames * LATENCY_BUFFERS;
				for (const PendingClick& click : pending) {
					int64_t frame = (int64_t)std::llround(((double)click.timeUs - streamStartUs) * sampleRate / 1000000.0) + latencyFrames;
					scheduled.push_back({frame, click.accent});
				}
				pending.clear();
				pendingMutex.unlock();
				std::sort(scheduled.begin(), scheduled.end(), [](const ScheduledClick& a, const ScheduledClick& b) { return a.frame < b.frame; });
			}
			// the volume changes smoothly, so that a click that sounds doesn't jump to the new volume
			float targetVolume = volume;
			float volumeCoef = 1.0f - (float)std::exp(-1.0 / (sampleRate * VOLUME_SMOOTH_SECS));
			int attackSamples = std::max(1, (int)(sampleRate * ATTACK_SECS));
			int clickSamples = std::max(attackSamples + 1, (int)(sampleRate * CLICK_SECS));
			size_t nextClick = 0;
			for (int frame = 0; frame < numFrames; frame++) {
				int64_t absoluteFrame = framesDone + frame;
				currentVolume += (targetVolume - currentVolume) * volumeCoef;
				// a click starts at its sample, from the start of its envelope and its waveform, and stops a click that still sounds
				// a click whose sample has passed (e.g. a beat triggered late) starts at once
				while (nextClick < scheduled.size() && scheduled[nextClick].frame <= absoluteFrame) {
					clickFreq = tune * (scheduled[nextClick].accent ? 2.0f : 1.0f);
					samplePos = 0;
					phase = 0.0;
					clicking = true;
					nextClick++;
				}
				if (!clicking) continue;
				if (samplePos >= clickSamples) {
					clicking = false;
					continue;
				}
				// a triangle from -1 to 1
				float triangle = (float)(4.0 * std::fabs(phase - 0.5) - 1.0);
				phase += clickFreq / sampleRate;
				if (phase >= 1.0) phase -= 1.0;
				// a short linear attack, so the click doesn't start with a jump, and an exponential decay
				float env;
				if (samplePos < attackSamples) env = (float)samplePos / (float)attackSamples;
				else env = std::exp(-6.0f * (float)(samplePos - attackSamples) / (float)(clickSamples - attackSamples));
				float sample = triangle * env * currentVolume;
				for (int chan = 0; chan < numChannels; chan++) {
					buffer[frame * numChannels + chan] += sample;
				}
				samplePos++;
			}
			scheduled.erase(scheduled.begin(), scheduled.begin() + nextClick);
			framesDone += numFrames;
		}

	private:
		struct PendingClick {
			int64_t timeUs;
			bool accent;
		};
		struct ScheduledClick {
			int64_t frame;
			bool accent;
		};
		static constexpr double ATTACK_SECS = 0.001;
		static constexpr double CLICK_SECS = 0.05;
		// how long a change of the volume takes, roughly
		static constexpr double VOLUME_SMOOTH_SECS = 0.005;
		// the clicks play this many buffers after their beats, so that they are scheduled before their samples are processed
		// despite the callbacks coming a bit early or late
		static constexpr int LATENCY_BUFFERS = 2;
		// how fast the time of the start of the stream follows the time of the callbacks, and how far off it is set again, in microseconds
		static constexpr double SMOOTHING = 0.01;
		static constexpr double RESYNC_US = 20000.0;
		// set by the main and sequencer threads, read by the audio thread
		std::atomic<bool> on{false};
		std::atomic<float> tune{440.0f};
		std::atomic<float> volume{0.5f};
		std::atomic<bool> clearScheduled{false};
		std::mutex pendingMutex;
		std::vector<PendingClick> pending;
		// used by the audio thread only
		std::vector<ScheduledClick> scheduled;
		int64_t framesDone = 0;
		double streamStartUs = 0.0;
		bool streamStartSet = false;
		double lastSampleRate = 0.0;
		bool clicking = false;
		float currentVolume = 0.5f;
		float clickFreq = 440.0f;
		int samplePos = 0;
		double phase = 0.0;
};

#endif
