#include <map>
#include <string>

// Python classes for the signal generators of Pyo (https://belangeo.github.io/pyo/api/classes/generators.html)
// made with the @pyoObj decorator (see pyoobj.py), by the lowercase name of the Pyo class
// every class plays its generator through an Adsr envelope, and has the same interface:
// setFreq() sets the frequency (the carrier of FM and CrossFM, the pitch of the chaotic attractors, and nothing for noise and audio input)
// setAmp() starts the envelope at an amplitude, and releases it with an amplitude of 0
// the decorator also makes a setter for every other parameter of build() (e.g. setRatio() for FM)
// the classes are named after the Pyo classes with "Synth" at the end, as a class with the same name (e.g. Sine)
// would replace the Pyo class it uses, and creating it would then call itself
std::map<std::string, std::string> pyoClasses = {
	{"blit", R"PY(@pyoObj
class BlitSynth:
	def build(self, freq=440, amp=0.5, harms=40):
		self.adsr = Adsr(mul=amp)
		self.blit = Blit(freq=freq, harms=harms, mul=self.adsr)
		return self.blit
	def setFreq(self, freq):
		self.blit.setFreq(freq)
	def setAmp(self, amp):
		if amp > 0:
			self.adsr.mul = amp
			self.adsr.play()
		else:
			self.adsr.stop())PY"},
	{"brownnoise", R"PY(@pyoObj
class BrownNoiseSynth:
	def build(self, amp=0.5):
		self.adsr = Adsr(mul=amp)
		self.brownnoise = BrownNoise(mul=self.adsr)
		return self.brownnoise
	def setFreq(self, freq):
		# noise has no frequency, this method is here so that all the classes can be used in the same way
		pass
	def setAmp(self, amp):
		if amp > 0:
			self.adsr.mul = amp
			self.adsr.play()
		else:
			self.adsr.stop())PY"},
	{"chenlee", R"PY(@pyoObj
class ChenLeeSynth:
	def build(self, pitch=0.25, amp=0.5, chaos=0.5, stereo=False):
		self.adsr = Adsr(mul=amp)
		self.chenlee = ChenLee(pitch=pitch, chaos=chaos, stereo=stereo, mul=self.adsr)
		return self.chenlee
	def setFreq(self, freq):
		# chaotic attractors have no frequency, their pitch (0 to 1) is the speed of their variations
		self.chenlee.setPitch(freq)
	def setAmp(self, amp):
		if amp > 0:
			self.adsr.mul = amp
			self.adsr.play()
		else:
			self.adsr.stop())PY"},
	{"crossfm", R"PY(@pyoObj
class CrossFMSynth:
	def build(self, freq=440, amp=0.5, ratio=0.5, ind1=2, ind2=2):
		self.adsr = Adsr(mul=amp)
		self.crossfm = CrossFM(carrier=freq, ratio=ratio, ind1=ind1, ind2=ind2, mul=self.adsr)
		return self.crossfm
	def setFreq(self, freq):
		self.crossfm.setCarrier(freq)
	def setAmp(self, amp):
		if amp > 0:
			self.adsr.mul = amp
			self.adsr.play()
		else:
			self.adsr.stop())PY"},
	{"fastsine", R"PY(@pyoObj
class FastSineSynth:
	def build(self, freq=440, amp=0.5):
		self.adsr = Adsr(mul=amp)
		self.fastsine = FastSine(freq=freq, mul=self.adsr)
		return self.fastsine
	def setFreq(self, freq):
		self.fastsine.setFreq(freq)
	def setAmp(self, amp):
		if amp > 0:
			self.adsr.mul = amp
			self.adsr.play()
		else:
			self.adsr.stop()
	def setQuality(self, x):
		self.fastsine.setQuality(x))PY"},
	{"fm", R"PY(@pyoObj
class FMSynth:
	def build(self, freq=440, amp=0.5, ratio=0.5, index=5):
		self.adsr = Adsr(mul=amp)
		self.fm = FM(carrier=freq, ratio=ratio, index=index, mul=self.adsr)
		return self.fm
	def setFreq(self, freq):
		self.fm.setCarrier(freq)
	def setAmp(self, amp):
		if amp > 0:
			self.adsr.mul = amp
			self.adsr.play()
		else:
			self.adsr.stop())PY"},
	{"input", R"PY(@pyoObj
class InputSynth:
	def build(self, amp=0.5):
		self.adsr = Adsr(mul=amp)
		self.input = Input(chnl=0, mul=self.adsr)
		return self.input
	def setFreq(self, freq):
		# an audio input has no frequency, this method is here so that all the classes can be used in the same way
		pass
	def setAmp(self, amp):
		if amp > 0:
			self.adsr.mul = amp
			self.adsr.play()
		else:
			self.adsr.stop())PY"},
	{"lfo", R"PY(@pyoObj
class LFOSynth:
	def build(self, freq=440, amp=0.5, sharp=0.5):
		self.adsr = Adsr(mul=amp)
		self.lfo = LFO(freq=freq, sharp=sharp, type=0, mul=self.adsr)
		return self.lfo
	def setFreq(self, freq):
		self.lfo.setFreq(freq)
	def setAmp(self, amp):
		if amp > 0:
			self.adsr.mul = amp
			self.adsr.play()
		else:
			self.adsr.stop()
	def setType(self, x):
		self.lfo.setType(x))PY"},
	{"lorenz", R"PY(@pyoObj
class LorenzSynth:
	def build(self, pitch=0.25, amp=0.5, chaos=0.5, stereo=False):
		self.adsr = Adsr(mul=amp)
		self.lorenz = Lorenz(pitch=pitch, chaos=chaos, stereo=stereo, mul=self.adsr)
		return self.lorenz
	def setFreq(self, freq):
		# chaotic attractors have no frequency, their pitch (0 to 1) is the speed of their variations
		self.lorenz.setPitch(freq)
	def setAmp(self, amp):
		if amp > 0:
			self.adsr.mul = amp
			self.adsr.play()
		else:
			self.adsr.stop())PY"},
	{"noise", R"PY(@pyoObj
class NoiseSynth:
	def build(self, amp=0.5):
		self.adsr = Adsr(mul=amp)
		self.noise = Noise(mul=self.adsr)
		return self.noise
	def setFreq(self, freq):
		# noise has no frequency, this method is here so that all the classes can be used in the same way
		pass
	def setAmp(self, amp):
		if amp > 0:
			self.adsr.mul = amp
			self.adsr.play()
		else:
			self.adsr.stop()
	def setType(self, x):
		self.noise.setType(x))PY"},
	{"phasor", R"PY(@pyoObj
class PhasorSynth:
	def build(self, freq=440, amp=0.5, phase=0):
		self.adsr = Adsr(mul=amp)
		self.phasor = Phasor(freq=freq, phase=phase, mul=self.adsr)
		return self.phasor
	def setFreq(self, freq):
		self.phasor.setFreq(freq)
	def setAmp(self, amp):
		if amp > 0:
			self.adsr.mul = amp
			self.adsr.play()
		else:
			self.adsr.stop())PY"},
	{"pinknoise", R"PY(@pyoObj
class PinkNoiseSynth:
	def build(self, amp=0.5):
		self.adsr = Adsr(mul=amp)
		self.pinknoise = PinkNoise(mul=self.adsr)
		return self.pinknoise
	def setFreq(self, freq):
		# noise has no frequency, this method is here so that all the classes can be used in the same way
		pass
	def setAmp(self, amp):
		if amp > 0:
			self.adsr.mul = amp
			self.adsr.play()
		else:
			self.adsr.stop())PY"},
	{"rcosc", R"PY(@pyoObj
class RCOscSynth:
	def build(self, freq=440, amp=0.5, sharp=0.25):
		self.adsr = Adsr(mul=amp)
		self.rcosc = RCOsc(freq=freq, sharp=sharp, mul=self.adsr)
		return self.rcosc
	def setFreq(self, freq):
		self.rcosc.setFreq(freq)
	def setAmp(self, amp):
		if amp > 0:
			self.adsr.mul = amp
			self.adsr.play()
		else:
			self.adsr.stop())PY"},
	{"rossler", R"PY(@pyoObj
class RosslerSynth:
	def build(self, pitch=0.25, amp=0.5, chaos=0.5, stereo=False):
		self.adsr = Adsr(mul=amp)
		self.rossler = Rossler(pitch=pitch, chaos=chaos, stereo=stereo, mul=self.adsr)
		return self.rossler
	def setFreq(self, freq):
		# chaotic attractors have no frequency, their pitch (0 to 1) is the speed of their variations
		self.rossler.setPitch(freq)
	def setAmp(self, amp):
		if amp > 0:
			self.adsr.mul = amp
			self.adsr.play()
		else:
			self.adsr.stop())PY"},
	{"sine", R"PY(@pyoObj
class SineSynth:
	def build(self, freq=440, amp=0.5, phase=0):
		self.adsr = Adsr(mul=amp)
		self.sine = Sine(freq=freq, phase=phase, mul=self.adsr)
		return self.sine
	def setFreq(self, freq):
		self.sine.setFreq(freq)
	def setAmp(self, amp):
		if amp > 0:
			self.adsr.mul = amp
			self.adsr.play()
		else:
			self.adsr.stop())PY"},
	{"sineloop", R"PY(@pyoObj
class SineLoopSynth:
	def build(self, freq=440, amp=0.5, feedback=0):
		self.adsr = Adsr(mul=amp)
		self.sineloop = SineLoop(freq=freq, feedback=feedback, mul=self.adsr)
		return self.sineloop
	def setFreq(self, freq):
		self.sineloop.setFreq(freq)
	def setAmp(self, amp):
		if amp > 0:
			self.adsr.mul = amp
			self.adsr.play()
		else:
			self.adsr.stop())PY"},
	{"sumosc", R"PY(@pyoObj
class SumOscSynth:
	def build(self, freq=440, amp=0.5, ratio=0.5, index=0.5):
		self.adsr = Adsr(mul=amp)
		self.sumosc = SumOsc(freq=freq, ratio=ratio, index=index, mul=self.adsr)
		return self.sumosc
	def setFreq(self, freq):
		self.sumosc.setFreq(freq)
	def setAmp(self, amp):
		if amp > 0:
			self.adsr.mul = amp
			self.adsr.play()
		else:
			self.adsr.stop())PY"},
	{"supersaw", R"PY(@pyoObj
class SuperSawSynth:
	def build(self, freq=440, amp=0.5, detune=0.5, bal=0.7):
		self.adsr = Adsr(mul=amp)
		self.supersaw = SuperSaw(freq=freq, detune=detune, bal=bal, mul=self.adsr)
		return self.supersaw
	def setFreq(self, freq):
		self.supersaw.setFreq(freq)
	def setAmp(self, amp):
		if amp > 0:
			self.adsr.mul = amp
			self.adsr.play()
		else:
			self.adsr.stop())PY"},
	{"samp", R"PY(@pyoObj
class SampPlayer:
	def build(self, path=None):
		self.snd_tab = SndTable(path, chnl=0)
		self.dur = self.snd_tab.getDur()
		self.trig = Trig()
		self.trigenv = TrigEnv(self.trig, self.snd_tab, self.dur)
		# the output is returned, and played with .out() on the object, like the other classes
		# (an attribute called out would replace the out() method of the object)
		self.output = Sig(self.trigenv).mix(2)
		return self.output
	def setSound(self, path):
		self.snd_tab.setSound(path)
		self.trigenv.setDur(self.snd_tab.getDur())
	def setFreq(self, freq):
		# a sound file has no frequency, this method is here so that all the classes can be used in the same way
		pass
	def setAmp(self, amp):
		# like the envelopes of the other classes, an amplitude greater than 0 starts the sound
		self.trigenv.mul = amp
		if amp > 0:
			self.trig.play()
	def trigger(self):
		# plays the sound from its beginning (a method called play() would replace play() of the object)
		self.trig.play())PY"}
};
