/* -*-c-*- */
/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* ---------------------------- included header files ----------------------- */

#include <config.h>

#include <stdio.h>

#include <X11/Xlib.h>

#include "fvwmlib.h"
#include "safemalloc.h"
#include "Strings.h"
#include "Flocale.h"
#include "Fft.h"
#include "FlocaleCharset.h"
#include "FftInterface.h"
#include "PictureBase.h"

/* ---------------------------- local definitions --------------------------- */

/* ---------------------------- local macros -------------------------------- */

#define FFT_SET_ROTATED_90_MATRIX(m) \
	((m)->xx = (m)->yy = 0, (m)->xy = 1, (m)->yx = -1)

#define FFT_SET_ROTATED_270_MATRIX(m) \
	((m)->xx = (m)->yy = 0, (m)->xy = -1, (m)->yx = 1)

#define FFT_SET_ROTATED_180_MATRIX(m) \
	((m)->xx = (m)->yy = -1, (m)->xy = (m)->yx = 0)

/* ---------------------------- imports ------------------------------------- */

/* ---------------------------- included code files ------------------------- */

/* ---------------------------- local types --------------------------------- */

/* ---------------------------- forward declarations ------------------------ */

/* ---------------------------- local variables ----------------------------- */

static Display *fftdpy = NULL;
static int fftscreen;
static int fft_initialized = False;

/* ---------------------------- exported variables (globals) ---------------- */

/* ---------------------------- local functions ----------------------------- */

static
void init_fft(Display *dpy)
{
	fftdpy = dpy;
	fftscreen = DefaultScreen(dpy);
	fft_initialized = True;
}

static
FftChar16 *FftUtf8ToFftString16(unsigned char *str, int len, int *nl)
{
	FftChar16 *new;
	int i = 0, j= 0;

	new = (FftChar16 *)safemalloc((len+1)*sizeof(FftChar16));
	while(i < len && str[i] != 0)
	{
		if (str[i] <= 0x7f)
		{
			new[j] = str[i];
		}
		else if (str[i] <= 0xdf && i+1 < len)
		{
		    new[j] = ((str[i] & 0x1f) << 6) + (str[i+1] & 0x3f);
		    i++;
		}
		else if (i+2 < len)
		{
			new[j] = ((str[i] & 0x0f) << 12) +
				((str[i+1] & 0x3f) << 6) +
				(str[i+2] & 0x3f);
			i += 2;
		}
		i++; j++;
	}
	*nl = j;
	return new;
}

static
void FftSetupEncoding(
	Display *dpy, FftFontType *fftf, char *encoding, char *module)
{
	FlocaleCharset *fc;

	if (!FftSupport || fftf == NULL)
		return;

	fftf->encoding = NULL;
	fftf->str_encoding = NULL;

	if (encoding != NULL)
	{
		fftf->str_encoding = encoding;
	}
	if (FftSupportUseXft2)
	{
		if (encoding != NULL)
		{
			fftf->encoding = encoding;
		}
		else if ((fc =
			  FlocaleCharsetGetDefaultCharset(dpy,NULL)) != NULL &&
			 StrEquals(fc->x,FLC_FFT_ENCODING_ISO8859_1))
		{
			fftf->encoding = FLC_FFT_ENCODING_ISO8859_1;
		}
		else
		{
			fftf->encoding = FLC_FFT_ENCODING_ISO10646_1;
		}
	}
	else if (FftSupport)
	{
		int i = 0;
		FftPatternElt *e;
		Fft1Font *f;

		f =  (Fft1Font *)fftf->fftfont;
		while(i < f->pattern->num)
		{
			e = &f->pattern->elts[i];
			if (StrEquals(e->object, FFT_ENCODING) &&
			    e->values->value.u.s != NULL)
			{
				fftf->encoding = e->values->value.u.s;
				return;
			}
			i++;
		}
	}
}

static
FftFont *FftGetRotatedFont(
	Display *dpy, FftFont *f, text_rotation_type text_rotation)
{
	FftPattern *rotated_pat;
	FftMatrix m;
	int dummy;

	if (f == NULL)
		return NULL;

	rotated_pat = FftPatternDuplicate(f->pattern);
	if (rotated_pat == NULL)
		return NULL;

	dummy = FftPatternDel(rotated_pat, FFT_MATRIX);

	if (text_rotation == TEXT_ROTATED_90)
	{
		FFT_SET_ROTATED_90_MATRIX(&m);
	}
	else if (text_rotation == TEXT_ROTATED_180)
	{
		FFT_SET_ROTATED_180_MATRIX(&m);
	}
	else if (text_rotation == TEXT_ROTATED_270)
	{
		FFT_SET_ROTATED_270_MATRIX(&m);
	}
	else
	{
		return NULL;
	}

	if (!FftPatternAddMatrix(rotated_pat, FFT_MATRIX, &m))
		return NULL;
	return FftFontOpenPattern(dpy, rotated_pat);
}

void FftPDumyFunc(void)
{
}

/* ---------------------------- interface functions ------------------------- */

void FftGetFontHeights(
	FftFontType *fftf, int *height, int *ascent, int *descent)
{
	/* fft font height may be > fftfont->ascent + fftfont->descent, this
	 * depends on the minspace value */
	*height = fftf->fftfont->height;
	*ascent = fftf->fftfont->ascent;
	*descent = fftf->fftfont->descent;

	return;
}

void FftGetFontWidths(
	FlocaleFont *flf, int *max_char_width)
{
	FGlyphInfo extents;

	/* FIXME:  max_char_width should not be use in the all fvwm !!!*/
	if (FftUtf8Support && FLC_ENCODING_TYPE_IS_UTF_8(flf->fc))
	{
		FftTextExtentsUtf8(fftdpy, flf->fftf.fftfont, "W", 1, &extents);
	}
	else
	{
		FftTextExtents8(fftdpy, flf->fftf.fftfont, "W", 1, &extents);
	}
	*max_char_width = extents.xOff;

	return;
}

FftFontType *FftGetFont(Display *dpy, char *fontname, char *module)
{
	FftFont *fftfont = NULL;
	FftFontType *fftf = NULL;
	char *fn = NULL, *str_enc = NULL;
	FlocaleCharset *fc;

	if (!FftSupport)
	{
		return NULL;
	}
	if (!fontname)
	{
		return NULL;
	}
	if (!fft_initialized)
	{
		init_fft(dpy);
	}
	/* Xft2 always load an USC-4 fonts, that we consider as an USC-2 font
	 * (i.e., an ISO106464-1 font) or an ISO8859-1 if the user ask for this
	 * or the locale charset is ISO8859-1.
	 * Xft1 support ISO106464-1, ISO8859-1 and others we load an
	 * ISO8859-1 font by default if the locale charset is ISO8859-1 and
	 * an ISO106464-1 one if this not the case */
	if (matchWildcards("*?8859-1*", fontname))
	{
		str_enc = FLC_FFT_ENCODING_ISO8859_1;
	}
	else if (matchWildcards("*?10646-1*", fontname))
	{
		str_enc = FLC_FFT_ENCODING_ISO10646_1;
	}
	if (!FftSupportUseXft2 && str_enc == NULL)
	{
		if ((fc = FlocaleCharsetGetFLCXOMCharset()) != NULL &&
		    StrEquals(fc->x,FLC_FFT_ENCODING_ISO8859_1))
		{
			fn = CatString2(fontname,":encoding=ISO8859-1");
		}
		else
		{
			fn = CatString2(fontname,":encoding=ISO10646-1");
		}
	}
	else
	{
		fn = fontname;
	}
	fftfont = FftFontOpenName(dpy, fftscreen, fn);
	if (!fftfont)
	{
		return NULL;
	}
	fftf = (FftFontType *)safemalloc(sizeof(FftFontType));
	fftf->fftfont = fftfont;
	fftf->fftfont_rotated_90 = NULL;
	fftf->fftfont_rotated_180 = NULL;
	fftf->fftfont_rotated_270 = NULL;
	FftSetupEncoding(dpy, fftf, str_enc, module);

	return fftf;
}

void FftDrawString(
	Display *dpy, FlocaleFont *flf, FlocaleWinString *fws,
	Pixel fg, Pixel fgsh, Bool has_fg_pixels, int len, unsigned long flags)
{
	FftDraw *fftdraw = NULL;
	void (*DrawStringFunc)();
	char *str;
	Bool free_str = False;
	XGCValues vr;
	XColor xfg, xfgsh;
	FftColor fft_fg, fft_fgsh;
	FftFontType *fftf;
	FftFont *uf;
	int x,y, xt,yt, step = 0;
	float alpha_factor;
	
	if (!FftSupport)
	{
		return;
	}

	fftf = &flf->fftf;
	if (fws->flags.text_rotation == TEXT_ROTATED_90) /* CW */
	{
		if (fftf->fftfont_rotated_90 == NULL)
		{
			fftf->fftfont_rotated_90 =
				FftGetRotatedFont(dpy, fftf->fftfont,
						  fws->flags.text_rotation);
		}
		uf = fftf->fftfont_rotated_90;
		y = fws->y;
		x = fws->x - FLF_SHADOW_BOTTOM_SIZE(flf);
	}
	else if (fws->flags.text_rotation == TEXT_ROTATED_180)
	{
		if (fftf->fftfont_rotated_180 == NULL)
		{
			fftf->fftfont_rotated_180 =
				FftGetRotatedFont(dpy, fftf->fftfont,
						  fws->flags.text_rotation);
		}
		uf = fftf->fftfont_rotated_180;
		y = fws->y;
		x = fws->x + FftTextWidth(flf, fws->e_str, len);
	}
	else if (fws->flags.text_rotation == TEXT_ROTATED_270)
	{
		if (fftf->fftfont_rotated_270 == NULL)
		{
			fftf->fftfont_rotated_270 =
				FftGetRotatedFont(dpy, fftf->fftfont,
						  fws->flags.text_rotation);
		}
		uf = fftf->fftfont_rotated_270;
		y = fws->y + FftTextWidth(flf, fws->e_str, len);
		x = fws->x - FLF_SHADOW_UPPER_SIZE(flf);
	}
	else
	{
		uf = fftf->fftfont;
		y = fws->y;
		x = fws->x;
	}

	if (uf == NULL)
		return;

	fftdraw = FftDrawCreate(dpy, (Drawable)fws->win, Pvisual, Pcmap);
	if (has_fg_pixels)
	{
		xfg.pixel = fg;
		xfgsh.pixel = fgsh;
	}
	else if (fws->gc &&
		 XGetGCValues(dpy, fws->gc, GCForeground, &vr))
	{
		xfg.pixel = vr.foreground;
	}
	else
	{
#if 0
		fprintf(stderr, "[fvwmlibs][FftDrawString]: ERROR --"
			" cannot found color\n");
#endif
		xfg.pixel = WhitePixel(dpy, fftscreen);
	}

	XQueryColor(dpy, Pcmap, &xfg);
	alpha_factor = ((fws->flags.has_colorset)?
		 ((float)fws->colorset->fg_alpha/100) : 1);
	/* Render uses premultiplied alpha */
	fft_fg.color.red = xfg.red * alpha_factor;
	fft_fg.color.green = xfg.green * alpha_factor;
	fft_fg.color.blue = xfg.blue * alpha_factor;
	fft_fg.color.alpha = 0xffff * alpha_factor;
	fft_fg.pixel = xfg.pixel;
	if (flf->shadow_size != 0 && has_fg_pixels)
	{
		XQueryColor(dpy, Pcmap, &xfgsh);
		fft_fgsh.color.red = xfgsh.red * alpha_factor;
		fft_fgsh.color.green = xfgsh.green * alpha_factor;
		fft_fgsh.color.blue = xfgsh.blue * alpha_factor;
		fft_fgsh.color.alpha = 0xffff * alpha_factor;
		fft_fgsh.pixel = xfgsh.pixel;
	}

	xt = x;
	yt = y;

	str = fws->e_str;
	if (FftUtf8Support && FLC_ENCODING_TYPE_IS_UTF_8(flf->fc))
	{
		DrawStringFunc = FftPDrawStringUtf8;
	}
	else if (FLC_ENCODING_TYPE_IS_UTF_8(flf->fc))
	{
		DrawStringFunc = FftPDrawString16;
		str = (char *)FftUtf8ToFftString16(
			  (unsigned char *)fws->e_str, len, &len);
		free_str = True;
	}
	else if (FLC_ENCODING_TYPE_IS_USC_2(flf->fc))
	{
		DrawStringFunc = FftPDrawString16;
	}
	else if (FLC_ENCODING_TYPE_IS_USC_4(flf->fc))
	{
		DrawStringFunc = FftPDrawString32;
	}
	else
	{
		DrawStringFunc = FftPDrawString8;
	}

	if (flf->shadow_size != 0 && has_fg_pixels)
	{
		while(FlocaleGetShadowTextPosition(flf, fws, x, y,
						   &xt, &yt, &step))
		{
			DrawStringFunc(fftdraw, &fft_fgsh, uf, xt, yt, str, len);
		}
	}
	DrawStringFunc(fftdraw, &fft_fg, uf, xt, yt, str, len);

	if (free_str && str != NULL)
	{
		free(str);
	}
	FftDrawDestroy (fftdraw);
}

int FftTextWidth(FlocaleFont *flf, char *str, int len)
{
	FGlyphInfo extents;
	int result = 0;

	if (!FftSupport)
	{
		return 0;
	}
	if (FftUtf8Support && FLC_ENCODING_TYPE_IS_UTF_8(flf->fc))
	{
		FftTextExtentsUtf8(
				fftdpy, flf->fftf.fftfont, str, len, &extents);
		result = extents.xOff;
	}
	else if (FLC_ENCODING_TYPE_IS_UTF_8(flf->fc))
	{
		FftChar16 *new;
		int nl;

		new = FftUtf8ToFftString16((unsigned char *)str, len, &nl);
		if (new != NULL)
		{
			FftTextExtents16(
				fftdpy, flf->fftf.fftfont, new, nl, &extents);
			result = extents.xOff;
			free(new);
		}
	}
	else if (FLC_ENCODING_TYPE_IS_USC_2(flf->fc))
	{
		FftTextExtents16(
			fftdpy, flf->fftf.fftfont, (FftChar16 *)str, len,
			&extents);
		result = extents.xOff;
	}
	else if (FLC_ENCODING_TYPE_IS_USC_4(flf->fc))
	{
		FftTextExtents32(
			fftdpy, flf->fftf.fftfont, (FftChar32 *)str, len,
			&extents);
		result = extents.xOff;
	}
	else
	{
		FftTextExtents8(
			fftdpy, flf->fftf.fftfont, (FftChar8 *)str, len,
			&extents);
		result = extents.xOff;
	}

	return result;
}