#include <assert.h>
#include <stdbool.h>
#include <math.h>

#include <fftw3.h>

#include "../../shared.h"

#define max(a,b) \
	 ({ __typeof__ (a) _a = (a); \
			 __typeof__ (b) _b = (b); \
		 _a > _b ? _a : _b; })

// exported function, a macro used to determine which functions
// are exposed as symbols within the final library/obj files
#define EXP_FUNC __attribute__ ((visibility ("default")))

// i wont even bother deciphering this mess
static float *fc, *fre, *fpeak, *k, g;
static int *f, *lcf, *hcf, *fmem, *flast, *flastd, *fall, *fl, *fr;
static double* smooth, gravity, integral, eqBalance, logScale, ignore;
static double monstercat, *peak, smh;
static bool oddoneout;
static uint32_t smcount, highcf, lowcf, waves, overshoot, calcbars;

static fftw_plan pl, pr;
fftw_complex *outl, *outr;

bool senseLow = true;
static float lastSens;

void separate_freq_bands(fftw_complex *out, int bars, int channel, double sens, double ignore, int fftsize) {
	int o,i;
	double y[fftsize / 2 + 1];
	double temp;

	// process: separate frequency bands
	for (o = 0; o < bars; o++) {
		peak[o] = 0;

		// process: get peaks
		for (i = lcf[o]; i <= hcf[o]; i++) {
			//getting r of compex
			y[i] = hypot(out[i][0], out[i][1]);
			peak[o] += y[i]; //adding upp band
		}

		peak[o] = peak[o] / (hcf[o]-lcf[o] + 1); //getting average
		temp = peak[o] * sens * k[o] / 800000; //multiplying with k and sens
		if (temp <= ignore) temp = 0;
		if (channel == 1) fl[o] = temp;
		else fr[o] = temp;
	}
} 


void monstercat_filter(int bars, int waves, double monstercat, int *data) {
	int z;

	// process [smoothing]: monstercat-style "average"
	int m_y, de;
	if (waves > 0) {
		for (z = 0; z < bars; z++) { // waves
			data[z] = data[z] / 1.25;
			//if (f[z] < 1) f[z] = 1;
			for (m_y = z - 1; m_y >= 0; m_y--) {
				de = z - m_y;
				data[m_y] = max(data[z] - pow(de, 2), data[m_y]);
			}
			for (m_y = z + 1; m_y < bars; m_y++) {
				de = m_y - z;
				data[m_y] = max(data[z] - pow(de, 2), data[m_y]);
			}
		}
	} else if (monstercat > 0) {
		for (z = 0; z < bars; z++) {
			//if (f[z] < 1)f[z] = 1;
			for (m_y = z - 1; m_y >= 0; m_y--) {
				de = z - m_y;
				data[m_y] = max(data[z] / pow(monstercat, de), data[m_y]);
			}
			for (m_y = z + 1; m_y < bars; m_y++) {
				de = m_y - z;
				data[m_y] = max(data[z] / pow(monstercat, de), data[m_y]);
			}
		}
	}
}

EXP_FUNC int xavaFilterInit(struct XAVA_HANDLE *hand) {
	struct audio_data *audio = &hand->audio;
	struct config_params *p  = &hand->conf;

	// fft: planning to rock
	outl = calloc(audio->fftsize/2+1, sizeof(fftw_complex));
	pl = fftw_plan_dft_r2c_1d(audio->fftsize, audio->audio_out_l, outl, FFTW_MEASURE);

	if(p->stereo) {
		outr = calloc(audio->fftsize/2+1, sizeof(fftw_complex));
		pr = fftw_plan_dft_r2c_1d(audio->fftsize, audio->audio_out_r, outr, FFTW_MEASURE);
	}

	xavaBailCondition(highcf > audio->rate / 2,
			"Higher cutoff cannot be higher than the sample rate / 2");

	fc = malloc(sizeof(float)*hand->bars+1);
	fre = malloc(sizeof(float)*hand->bars+1);
	fpeak = malloc(sizeof(float)*hand->bars+1);
	k = malloc(sizeof(float)*hand->bars+1);
	f = malloc(sizeof(int)*hand->bars+1);
	lcf = malloc(sizeof(int)*hand->bars+1);
	hcf = malloc(sizeof(int)*hand->bars+1);
	fmem = malloc(sizeof(int)*hand->bars+1);
	flast = malloc(sizeof(int)*hand->bars+1);
	flastd = malloc(sizeof(int)*hand->bars+1);
	fall = malloc(sizeof(int)*hand->bars+1);
	fl = malloc(sizeof(int)*hand->bars+1);
	fr = malloc(sizeof(int)*hand->bars+1);
	peak = malloc(sizeof(double)*(hand->bars+1)+1);

	return 0;
}

EXP_FUNC void xavaFilterApply(struct XAVA_HANDLE *hand) {
	struct audio_data *audio = &hand->audio;
	struct config_params *p  = &hand->conf;

	// if fc is not cleared that means that the variables are not initialized
	void *temp;
	temp = realloc(fc,sizeof(float)*hand->bars+1);assert(temp);fc=temp;
	temp = realloc(fre,sizeof(float)*hand->bars+1);assert(temp);fre=temp;
	temp = realloc(fpeak,sizeof(float)*hand->bars+1);assert(temp);fpeak=temp;
	temp = realloc(k,sizeof(float)*hand->bars+1);assert(temp);k=temp;
	temp = realloc(f,sizeof(int)*hand->bars+1);assert(temp);f=temp;
	temp = realloc(lcf,sizeof(int)*hand->bars+1);assert(temp);lcf=temp;
	temp = realloc(hcf,sizeof(int)*hand->bars+1);assert(temp);hcf=temp;
	temp = realloc(fmem,sizeof(int)*hand->bars+1);assert(temp);fmem=temp;
	temp = realloc(flast,sizeof(int)*hand->bars+1);assert(temp);flast=temp;
	temp = realloc(flastd,sizeof(int)*hand->bars+1);assert(temp);flastd=temp;
	temp = realloc(fall,sizeof(int)*hand->bars+1);assert(temp);fall=temp;
	temp = realloc(fl,sizeof(int)*hand->bars+1);assert(temp);fl=temp;
	temp = realloc(fr,sizeof(int)*hand->bars+1);assert(temp);fr=temp;
	temp = realloc(peak,sizeof(double)*(hand->bars+1)+1);assert(temp);peak=temp;

	// oddoneout only works if the number of bars is odd, go figure
	if(oddoneout) {
		if (!(hand->bars%2))
			hand->bars--;
	}

	// update pointers for the main handle
	hand->f  = f;
	hand->fl = flastd;

	for (int i = 0; i < hand->bars; i++) {
		flast[i] = 0;
		flastd[i] = 0;
		fall[i] = 0;
		fpeak[i] = 0;
		fmem[i] = 0;
		f[i] = 0;
		lcf[i] = 0;
		hcf[i] = 0;
		fl[i] = 0;
		fr[i] = 0;
		fc[i] = .0;
		fre[i] = .0;
		fpeak[i] = .0;
		k[i] = .0;
	}

	// so apparently sens can INDEED reach infinity
	// so I've decided to limit how high sens can rise
	// to prevent sens from reaching infinity again
	if(!hand->pauseRendering)
		lastSens = p->sens;

	// process [smoothing]: calculate gravity
	g = gravity * ((float)(p->h-1) / 2160) * (60 / (float)p->framerate);

	// checks if there is stil extra room, will use this to center
	hand->rest = (p->w - hand->bars * p->bw - hand->bars * p->bs + p->bs) / 2;
	if(hand->rest < 0)
		hand->rest = 0;

	xavaSpam("w=%d, h=%d, bars=%d, bw=%d, rest=%d",
				p->w, p->h, hand->bars, p->bw, hand->rest);

	if (p->stereo) hand->bars = hand->bars / 2; // in stereo onle half number of bars per channel

	if ((smcount > 0) && (hand->bars > 0)) {
		smh = (double)(((double)smcount)/((double)hand->bars));
	}

	// since oddoneout requires every odd bar, why not split them in half?
	calcbars = oddoneout ? hand->bars/2+1 : hand->bars;

	// frequency constant that we'll use for logarithmic progression of frequencies
	double freqconst = log(highcf-lowcf)/log(pow(calcbars, logScale));
	//freqconst = -2;

	// process: calculate cutoff frequencies
	int n;
	for (n=0; n < calcbars; n++) {
		fc[n] = pow(powf(n, (logScale-1.0)*((double)n+1.0)/((double)calcbars)+1.0),
						 freqconst)+lowcf;
		fre[n] = fc[n] / (audio->rate / 2.0); 
		// Remember nyquist!, pr my calculations this should be rate/2 
		// and  nyquist freq in M/2 but testing shows it is not... 
		// or maybe the nq freq is in M/4

		//lfc stores the lower cut frequency foo each bar in the fft out buffer
		lcf[n] = floor(fre[n] * (audio->fftsize/2.0));

		if (n != 0) {
			//hfc holds the high cut frequency for each bar

			// I know it's not precise, but neither are integers
			// You can see why in https://github.com/nikp123/xava/issues/29
			// I did reverse the "next_bar_lcf-1" change
			hcf[n-1] = lcf[n]; 
		}

		if (n != 0) {
			xavaSpam("%d: %f -> %f (%d -> %d)", 
				n, fc[n - 1], fc[n], lcf[n - 1], hcf[n - 1]);
		}
	}
	hcf[n-1] = highcf*audio->fftsize/audio->rate;

	// process: weigh signal to frequencies height and EQ
	for (n = 0; n < calcbars; n++) {
		k[n] = pow(fc[n], eqBalance);
		k[n] *= (float)(p->h-1) / 100;
		k[n] *= smooth[(int)floor(((double)n) * smh)];
	}

	if (p->stereo)
		hand->bars = hand->bars * 2;
}

EXP_FUNC void xavaFilterLoop(struct XAVA_HANDLE *hand) {
	struct audio_data *audio = &hand->audio;
	struct config_params *p  = &hand->conf;
	int i;

	// process: execute FFT and sort frequency bands
	if (p->stereo) {
		fftw_execute(pl);
		fftw_execute(pr);

		separate_freq_bands(outl, calcbars, 1, p->sens, ignore, audio->fftsize);
		separate_freq_bands(outr, calcbars, 2, p->sens, ignore, audio->fftsize);
	} else {
		fftw_execute(pl);
		separate_freq_bands(outl, calcbars, 1, p->sens, ignore, audio->fftsize);
	}

	if(oddoneout) {
		for(int i=hand->bars/2; i>0; i--) {
			fl[i*2-1+hand->bars%2]=fl[i];
			fl[i]=0;
		}
	}

	// process [smoothing]
	if (monstercat) {
		if (p->stereo) {
			monstercat_filter(hand->bars / 2, waves, monstercat, fl);
			monstercat_filter(hand->bars / 2, waves, monstercat, fr);
		} else {
			monstercat_filter(calcbars, waves, monstercat, fl);
		}
	}

	//preperaing signal for drawing
	for (i=0; i<hand->bars; i++) {
		if (p->stereo) {
			if (i < hand->bars / 2) {
				f[i] = fl[hand->bars / 2 - i - 1];
			} else {
				f[i] = fr[i - hand->bars / 2];
			}
		} else {
			f[i] = fl[i];
		}
	}


	// process [smoothing]: falloff
	if (g > 0) {
		for (i = 0; i < hand->bars; i++) {
			if (f[i] < flast[i]) {
				f[i] = fpeak[i] - (g * fall[i] * fall[i]);
				fall[i]++;
			} else  {
				fpeak[i] = f[i];
				fall[i] = 0;
			}
			flast[i] = f[i];
		}
	}

	// process [smoothing]: integral
	if (integral > 0) {
		for (i=0; i<hand->bars; i++) {
			f[i] = fmem[i] * integral + f[i];
			fmem[i] = f[i];

			int diff = p->h - f[i]; 
			if (diff < 0) diff = 0;
			double div = 1.0 / (double)(diff + 1);
			//f[o] = f[o] - pow(div, 10) * (height + 1); 
			fmem[i] = fmem[i] * (1 - div / 20); 
		}
	}

	// process [oddoneout]
	if(oddoneout) {
		for(i=1; i<hand->bars-1; i+=2) {
			f[i] = f[i+1]/2 + f[i-1]/2;
		}
		for(i=hand->bars-3; i>1; i-=2) {
			int sum = f[i+1]/2 + f[i-1]/2;
			if(sum>f[i]) f[i] = sum;
		}
	}

	// automatic sens adjustment
	if (p->autosens&&(!hand->pauseRendering)) {
		// don't adjust on complete silence
		// as when switching tracks for example
		for (i=0; i<hand->bars; i++) {
			if (f[i] > p->h-1) {
				senseLow = false;
				p->sens *= 0.985;
				break;
			}
			if (senseLow) {
				p->sens *= 1.01;
				// impose artificial limit on sens growth
				if(p->sens > lastSens*2) p->sens = lastSens*2;
			}
		}
	}
}

EXP_FUNC void xavaFilterCleanup(struct XAVA_HANDLE *hand) {
	free(outl);
	free(outr);
	fftw_destroy_plan(pl);
	fftw_destroy_plan(pr);
	fftw_cleanup();

	free(smooth);

	// honestly even I don't know what these mean
	// but for the meantime, they are moved here and I won't touch 'em
	free(fc);
	free(fre);
	free(fpeak);
	free(k);
	free(f);
	free(lcf);
	free(hcf);
	free(fmem);
	free(flast);
	free(flastd);
	free(fall);
	free(fl);
	free(fr);
	free(peak);
}

EXP_FUNC void xavaFilterHandleConfiguration(struct XAVA_HANDLE *hand, void *dictHand) {
	dictionary *ini = dictHand;

	struct config_params *p = &hand->conf;

	p->sens =      iniparser_getdouble(ini, "filter:sensitivity", 100.0) *
		XAVA_PREDEFINED_SENS_VALUE; // check shared.h for details
	p->autosens = iniparser_getboolean(ini, "filter:autosens", 1);
	overshoot =       iniparser_getint(ini, "filter:overshoot", 0);
	lowcf =           iniparser_getint(ini, "filter:lower_cutoff_freq", 26);
	highcf =          iniparser_getint(ini, "filter:higher_cutoff_freq", 15000);

	monstercat = iniparser_getdouble(ini, "filter:monstercat", 0.0) * 1.5;
	waves =         iniparser_getint(ini, "filter:waves", 0);
	integral =   iniparser_getdouble(ini, "filter:integral", 85);
	gravity =    iniparser_getdouble(ini, "filter:gravity", 100) * 50.0;
	ignore =     iniparser_getdouble(ini, "filter:ignore", 0);
	logScale =   iniparser_getdouble(ini, "filter:log", 1.55);
	oddoneout = iniparser_getboolean(ini, "filter:oddoneout", 1);
	eqBalance =  iniparser_getdouble(ini, "filter:eq_balance", 0.67);

	// read & validate: eq
	smcount = iniparser_getsecnkeys(ini, "eq");
	if (smcount > 0) {
		smooth = malloc(smcount*sizeof(double));
		#ifndef LEGACYINIPARSER
		const char *keys[smcount];
		iniparser_getseckeys(ini, "eq", keys);
		#else
		char **keys = iniparser_getseckeys(ini, "eq");
		#endif
		for (int sk = 0; sk < smcount; sk++) {
			smooth[sk] = iniparser_getdouble(ini, keys[sk], 1);
		}
	} else {
		smcount = 64; //back to the default one
		smooth = malloc(smcount*sizeof(double));
		for(int i=0; i<64; i++) smooth[i]=1.0f;
	}

	// validate: gravity
	gravity = gravity / 100;
	if (gravity < 0) {
		xavaWarn("Gravity cannot be below 0");
		gravity = 0;
	} 

	// validate: oddoneout
	xavaBailCondition((p->stereo&&oddoneout), 
			"'oddoneout' and stereo channels do not work together!"); 

	// validate: integral
	integral = integral / 100;
	if (integral < 0) {
		xavaWarn("Integral cannot be below 0");
		integral = 0;
	} else if (integral > 1) {
		xavaWarn("Integral cannot be above 100");
		integral = 1;
	}

	// validate: cutoff
	if (lowcf == 0 ) lowcf++;
	xavaBailCondition(lowcf > highcf,
			"Lower frequency cutoff cannot be higher than the higher cutoff\n");
}
