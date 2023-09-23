#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <list>
#include <map>
#include <string>
#include <vector>

#include <gtk/gtk.h>
#include <vte/vte.h>

#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif

#include "util/maybe.hh"
#include "util/memory.hh"

#include "mode/modes.hh"

#include <fstream>
#include <sys/stat.h>

using namespace std::placeholders;
using std::string;
using std::map;
using std::list;

/* Allow scales a bit smaller and a bit larger than the usual pango ranges */
#define TERMINAL_SCALE_XXX_SMALL   (PANGO_SCALE_XX_SMALL/1.2)
#define TERMINAL_SCALE_XXXX_SMALL  (TERMINAL_SCALE_XXX_SMALL/1.2)
#define TERMINAL_SCALE_XXXXX_SMALL (TERMINAL_SCALE_XXXX_SMALL/1.2)
#define TERMINAL_SCALE_XXX_LARGE   (PANGO_SCALE_XX_LARGE*1.2)
#define TERMINAL_SCALE_XXXX_LARGE  (TERMINAL_SCALE_XXX_LARGE*1.2)
#define TERMINAL_SCALE_XXXXX_LARGE (TERMINAL_SCALE_XXXX_LARGE*1.2)
#define TERMINAL_SCALE_MINIMUM     (TERMINAL_SCALE_XXXXX_SMALL/1.2)
#define TERMINAL_SCALE_MAXIMUM     (TERMINAL_SCALE_XXXXX_LARGE*1.2)

#define ZXY(A) Mode A(#A, A##ƌ, A##Shiftƌ);
#define QYX(A) Mode A(#A, Q##A##ƌ, Q##A##Shiftƌ);


static string mode = "normal";
static string winTitle = "none";

static const std::vector<double> zoom_factors = {
    TERMINAL_SCALE_MINIMUM,
    TERMINAL_SCALE_XXXXX_SMALL,
    TERMINAL_SCALE_XXXX_SMALL,
    TERMINAL_SCALE_XXX_SMALL,
    PANGO_SCALE_XX_SMALL,
    PANGO_SCALE_X_SMALL,
    PANGO_SCALE_SMALL,
    PANGO_SCALE_MEDIUM,
    PANGO_SCALE_LARGE,
    PANGO_SCALE_X_LARGE,
    PANGO_SCALE_XX_LARGE,
    TERMINAL_SCALE_XXX_LARGE,
    TERMINAL_SCALE_XXXX_LARGE,
    TERMINAL_SCALE_XXXXX_LARGE,
    TERMINAL_SCALE_MAXIMUM
};

class Mode {

    public:
    list<char> seq;
    map<int, const char*> ƌ;
    map<int, const char*> shiftƌ;

    Mode(string c, map<int, const char*> ƌ1, map<int, const char*> shiftƌ1) {
        list<char> lc;
        if(c.size() == 3) {
            lc.push_front(c[2]);
            lc.push_front(c[1]);
            lc.push_front(c[0]);
        } else if(c.size() == 2) {
            lc.push_front(c[1]);
            lc.push_front(c[0]);
        } else {
            lc.push_front(c[0]);
        }
        seq = lc;
        ƌ = ƌ1;
        shiftƌ = shiftƌ1;
    }
};

struct config_info {
    char *browser;
    gboolean dynamic_title, urgent_on_bell;
    gboolean modify_other_keys;
    int tag;
    char *config_file;
    gdouble font_scale;
};

struct keybind_info {
    GtkWindow *window;
    VteTerminal *vte;
    config_info config;
};


static void window_title_cb(VteTerminal *vte, gboolean *dynamic_title);
static gboolean key_press_cb(VteTerminal *vte, GdkEventKey *event, keybind_info *info);
static gboolean position_overlay_cb(GtkBin *overlay, GtkWidget *widget, GdkRectangle *alloc);

static void load_config(GtkWindow *window, VteTerminal *vte, config_info *info, char **geometry, char **icon);
static void set_config(GtkWindow *window, VteTerminal *vte, config_info *info, char **geometry, char **icon, GKeyFile *config);

static void override_background_color(GtkWidget *widget, GdkRGBA *rgba) {
    GtkCssProvider *provider = gtk_css_provider_new();

    gchar *colorstr = gdk_rgba_to_string(rgba);
    char *css = g_strdup_printf("* { background-color: %s; }", colorstr);
    gtk_css_provider_load_from_data(provider, css, -1, nullptr);
    g_free(colorstr);
    g_free(css);

    gtk_style_context_add_provider(gtk_widget_get_style_context(widget),
                                   GTK_STYLE_PROVIDER(provider),
                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

static list<char> keySeq;
std::ofstream activityFile;
std::ifstream fontFile;
static VteTerminal* VTEGLOBE;
static GtkWidget* WINGLOBE;
FILE* receiveFile;

static int recPos = 0;
static int pausePty = 0;

static int focus = 0;

static long originCol = 0;
static long originRow = 0;

/* std::ofstream activityFile; */
/* std::ifstream fontFile; */

static list<char> qKeySeq;
static int useQ = 0;

static const std::map<int, const char *> keyƌ = {
    { GDK_KEY_Tab,        "<Tab>"  },
    { GDK_KEY_Return,     "<CR>" },
    { GDK_KEY_i,     "i" },
    { GDK_KEY_period,     "." },
    { GDK_KEY_slash,     "/" },
    { GDK_KEY_0,          "0" },
    { GDK_KEY_1,          "1" },
    { GDK_KEY_9,          "9" },
};

static const std::map<int, const char *> modify_table = {
    { GDK_KEY_Tab,        "\033[27;5;9~"  },
    { GDK_KEY_Return,     "\033[27;5;13~" },
    { GDK_KEY_apostrophe, "\033[27;5;39~" },
    { GDK_KEY_comma,      "\033[27;5;44~" },
    { GDK_KEY_minus,      "\033[27;5;45~" },
    { GDK_KEY_period,     "\033[27;5;46~" },
    { GDK_KEY_0,          "\033[27;5;48~" },
    { GDK_KEY_1,          "\033[27;5;49~" },
    { GDK_KEY_9,          "\033[27;5;57~" },
    { GDK_KEY_semicolon,  "\033[27;5;59~" },
    { GDK_KEY_equal,      "\033[27;5;61~" },
    { GDK_KEY_exclam,     "\033[27;6;33~" },
    { GDK_KEY_quotedbl,   "\033[27;6;34~" },
    { GDK_KEY_numbersign, "\033[27;6;35~" },
    { GDK_KEY_dollar,     "\033[27;6;36~" },
    { GDK_KEY_percent,    "\033[27;6;37~" },
    { GDK_KEY_ampersand,  "\033[27;6;38~" },
    { GDK_KEY_parenleft,  "\033[27;6;40~" },
    { GDK_KEY_parenright, "\033[27;6;41~" },
    { GDK_KEY_asterisk,   "\033[27;6;42~" },
    { GDK_KEY_plus,       "\033[27;6;43~" },
    { GDK_KEY_colon,      "\033[27;6;58~" },
    { GDK_KEY_less,       "\033[27;6;60~" },
    { GDK_KEY_greater,    "\033[27;6;62~" },
    { GDK_KEY_question,   "\033[27;6;63~" },
};

    // USED ^\, ^^, ^_ 
    // USED M-\,
    //{ GDK_KEY_comma,      "\x1F"  }, //^_,
static const std::map<int, const char *> modifyControlTable = {
    { GDK_KEY_semicolon,  "\x1C"  }, //^\,
    { GDK_KEY_apostrophe, "\x1B\\"}, //M_\,
    { GDK_KEY_m,          "\x7F"  }, //BS,
    { GDK_KEY_comma,      "\x1BOP"  }, //F1
    { GDK_KEY_Return,     "\x1BOS"  }, //F4
    { GDK_KEY_bracketright,"\x1B[17~"  }, //F6
};

static const std::map<int, const char *> modify_meta_table = {
    { GDK_KEY_Tab,        "\033[27;13;9~"  },
    { GDK_KEY_Return,     "\033[27;13;13~" },
    { GDK_KEY_apostrophe, "\033[27;13;39~" },
    { GDK_KEY_comma,      "\033[27;13;44~" },
    { GDK_KEY_minus,      "\033[27;13;45~" },
    { GDK_KEY_period,     "\033[27;13;46~" },
    { GDK_KEY_0,          "\033[27;13;48~" },
    { GDK_KEY_1,          "\033[27;13;49~" },
    { GDK_KEY_9,          "\033[27;13;57~" },
    { GDK_KEY_semicolon,  "\033[27;13;59~" },
    { GDK_KEY_equal,      "\033[27;13;61~" },
    { GDK_KEY_exclam,     "\033[27;14;33~" },
    { GDK_KEY_quotedbl,   "\033[27;14;34~" },
    { GDK_KEY_numbersign, "\033[27;14;35~" },
    { GDK_KEY_dollar,     "\033[27;14;36~" },
    { GDK_KEY_percent,    "\033[27;14;37~" },
    { GDK_KEY_ampersand,  "\033[27;14;38~" },
    { GDK_KEY_parenleft,  "\033[27;14;40~" },
    { GDK_KEY_parenright, "\033[27;14;41~" },
    { GDK_KEY_asterisk,   "\033[27;14;42~" },
    { GDK_KEY_plus,       "\033[27;14;43~" },
    { GDK_KEY_colon,      "\033[27;14;58~" },
    { GDK_KEY_less,       "\033[27;14;60~" },
    { GDK_KEY_greater,    "\033[27;14;62~" },
    { GDK_KEY_question,   "\033[27;14;63~" },
};

ZXY(a)
ZXY(aa)
ZXY(t)
ZXY(tt)
ZXY(i)
ZXY(ii)
ZXY(s)
ZXY(ss)
ZXY(D)
ZXY(DD)
ZXY(b)
ZXY(bb)
ZXY(g)
ZXY(gg)
ZXY(r)
ZXY(rr)
ZXY(m)
ZXY(mm)

ZXY(ab)
ZXY(aD)
ZXY(ai)
ZXY(as)
ZXY(at)

ZXY(ba)
ZXY(bD)
ZXY(bi)
ZXY(bs)
ZXY(bt)

ZXY(Da)
ZXY(Db)
ZXY(Di)
ZXY(Ds)
ZXY(Dt)

ZXY(ia)
ZXY(ib)
ZXY(iD)
ZXY(is)
ZXY(it)

ZXY(sa)
ZXY(sb)
ZXY(sD)
ZXY(si)
ZXY(st)

ZXY(ta)
ZXY(tb)
ZXY(tD)
ZXY(ti)
ZXY(ts)

ZXY(ag)
ZXY(am)
ZXY(ar)
ZXY(bg)
ZXY(bm)
ZXY(br)
ZXY(Dg)
ZXY(Dm)
ZXY(Dr)
ZXY(ig)
ZXY(im)
ZXY(ir)
ZXY(sg)
ZXY(sm)
ZXY(sr)
ZXY(tg)
ZXY(tm)
ZXY(tr)

ZXY(ga)
ZXY(gb)
ZXY(gD)
ZXY(gi)
ZXY(gm)
ZXY(gr)
ZXY(gs)
ZXY(gt)

ZXY(ma)
ZXY(mb)
ZXY(mD)
ZXY(mg)
ZXY(mi)
ZXY(mr)
ZXY(ms)
ZXY(mt)

ZXY(ra)
ZXY(rb)
ZXY(rD)
ZXY(rg)
ZXY(ri)
ZXY(rm)
ZXY(rs)
ZXY(rt)

ZXY(ac)
ZXY(bc)
ZXY(Dc)
ZXY(gc)
ZXY(ic)
ZXY(mc)
ZXY(rc)
ZXY(sc)
ZXY(tc)

ZXY(af)
ZXY(bf)
ZXY(Df)
ZXY(gf)
Mode ifZ("if", ifƌ, ifShiftƌ);
ZXY(mf)
ZXY(rf)
ZXY(sf)
ZXY(tf)

ZXY(ad)
ZXY(bd)
ZXY(Dd)
ZXY(gd)
ZXY(id)
ZXY(md)
ZXY(rd)
ZXY(sd)
ZXY(td)

ZXY(au)
ZXY(bu)
ZXY(Du)
ZXY(gu)
ZXY(iu)
ZXY(mu)
ZXY(ru)
ZXY(su)
ZXY(tu)

ZXY(ay)
ZXY(by)
ZXY(Dy)
ZXY(gy)
ZXY(iy)
ZXY(my)
ZXY(ry)
ZXY(sy)
ZXY(ty)

ZXY(ae)
ZXY(be)
ZXY(De)
ZXY(ge)
ZXY(ie)
ZXY(me)
ZXY(re)
ZXY(se)
ZXY(te)

ZXY(aw)
ZXY(bw)
ZXY(Dw)
ZXY(gw)
ZXY(iw)
ZXY(mw)
ZXY(rw)
ZXY(sw)
ZXY(tw)

ZXY(av)
ZXY(bv)
ZXY(Dv)
ZXY(gv)
ZXY(iv)
ZXY(mv)
ZXY(rv)
ZXY(sv)
ZXY(tv)

ZXY(y)

ZXY(yy)
ZXY(ya)
ZXY(yb)
ZXY(yc)
ZXY(yd)
ZXY(yD)
ZXY(ye)
ZXY(yf)
ZXY(yg)
ZXY(yi)
ZXY(ym)
ZXY(yr)
ZXY(ys)
ZXY(yu)
ZXY(yv)
ZXY(yw)

ZXY(ap)
ZXY(bp)
ZXY(Dp)
ZXY(gp)
ZXY(ip)
ZXY(mp)
ZXY(rp)
ZXY(sp)
ZXY(tp)
ZXY(yp)

ZXY(an)
ZXY(bn)
ZXY(Dn)
ZXY(gn)
ZXY(in)
ZXY(mn)
ZXY(rn)
ZXY(sn)
ZXY(tn)
ZXY(yn)

ZXY(ee)
ZXY(ea)
ZXY(eb)
ZXY(ec)
ZXY(ed)
ZXY(eD)
ZXY(ef)
ZXY(eg)
ZXY(ei)
ZXY(em)
ZXY(en)
ZXY(ep)
ZXY(er)
ZXY(es)
ZXY(et)
ZXY(eu)
ZXY(ev)
ZXY(ew)
ZXY(ey)

ZXY(wa)
ZXY(wb)
ZXY(wc)
ZXY(wD)
ZXY(we)
ZXY(wf)
ZXY(wg)
ZXY(wi)
ZXY(wm)
ZXY(wn)
ZXY(wp)
ZXY(wr)
ZXY(ws)
ZXY(wt)
ZXY(wu)
ZXY(wv)
ZXY(wy)

ZXY(aaf)
ZXY(aau)
ZXY(aav)
ZXY(aay)
ZXY(abb)
ZXY(aby)
ZXY(afv)
ZXY(agb)
ZXY(age)
ZXY(agg)
ZXY(agp)
ZXY(agu)
ZXY(agw)
ZXY(agy)

ZXY(bbb)
ZXY(bbf)
ZXY(bbg)
ZXY(bbm)
ZXY(bbp)
ZXY(bbv)
ZXY(bbw)
ZXY(bby)
ZXY(bDv)
ZXY(bib)
ZXY(big)
ZXY(bii)
ZXY(bip)
ZXY(biv)
ZXY(bfd)

ZXY(Dag)
ZXY(Dbb)
ZXY(Dbf)
ZXY(Dbg)
ZXY(Dbw)
ZXY(DDy)
ZXY(Dgb)
ZXY(Dgf)
ZXY(Dgt)
ZXY(Dgu)
ZXY(Dgw)
ZXY(Dmb)
ZXY(Dmc)
ZXY(Dmf)
ZXY(Dmp)
ZXY(Dmt)
ZXY(Dmu)
ZXY(Dmw)
ZXY(Dmy)
ZXY(Dsb)
ZXY(Dsf)
ZXY(Dsg)
ZXY(Dsw)
ZXY(Dsy)

ZXY(eav)
ZXY(ebv)
ZXY(eDv)
ZXY(eiv)
ZXY(esv)
ZXY(etv)

ZXY(gab)
ZXY(gac)
ZXY(gae)
ZXY(gaf)
ZXY(gag)
ZXY(gap)
ZXY(gar)
ZXY(gat)
ZXY(gau)
ZXY(gav)
ZXY(gaw)
ZXY(gay)
ZXY(gbb)
ZXY(gbe)
ZXY(gbg)
ZXY(gbp)
ZXY(gbr)
ZXY(gbt)
ZXY(gbu)
ZXY(gbv)
ZXY(gbw)
ZXY(gby)
ZXY(gib)
ZXY(gic)
ZXY(gie)
ZXY(gif)
ZXY(gig)
ZXY(gin)
ZXY(gip)
ZXY(gir)
ZXY(git)
ZXY(giu)
ZXY(giv)
ZXY(giw)
ZXY(giy)
ZXY(gpb)
ZXY(gpe)
ZXY(gpf)
ZXY(gmc)
ZXY(gmv)
ZXY(gsb)
ZXY(gsf)
ZXY(gsg)
ZXY(gsp)
ZXY(gst)
ZXY(gsu)
ZXY(gsw)
ZXY(guu)
ZXY(guv)
ZXY(gyp)

ZXY(ibv)
ZXY(iDc)

ZXY(mvf)

ZXY(rab)
ZXY(rac)
ZXY(rae)
ZXY(raf)
ZXY(rag)
ZXY(rap)
ZXY(rar)
ZXY(rat)
ZXY(rau)
ZXY(rav)
ZXY(raw)
ZXY(ray)
ZXY(rbb)
ZXY(rbc)
ZXY(rbf)
ZXY(rbg)
ZXY(rbi)
ZXY(rbp)
ZXY(rbr)
ZXY(rbt)
ZXY(rbu)
ZXY(rbv)
ZXY(rbw)
ZXY(rby)
ZXY(rDb)
ZXY(rDe)
ZXY(rDg)
ZXY(rDt)
ZXY(rDu)
ZXY(rDv)
ZXY(rDw)
ZXY(rDy)
ZXY(rfb)
ZXY(rfe)
ZXY(rff)
ZXY(rfg)
ZXY(rfp)
ZXY(rft)
ZXY(rfv)
ZXY(rfw)
ZXY(rge)
ZXY(rgg)
ZXY(rgt)
ZXY(rib)
ZXY(rig)
ZXY(rip)
ZXY(rir)
ZXY(rit)
ZXY(riv)
ZXY(riw)
ZXY(riy)
ZXY(rsb)
ZXY(rsc)
ZXY(rse)
ZXY(rsf)
ZXY(rsg)
ZXY(rsp)
ZXY(rsr)
ZXY(rst)
ZXY(rsu)
ZXY(rsv)
ZXY(rsw)
ZXY(rsy)
ZXY(rtb)
ZXY(rtc)
ZXY(rtf)
ZXY(rtg)
ZXY(rtr)
ZXY(rtt)
ZXY(rtv)
ZXY(rtw)
ZXY(rty)
ZXY(rug)
ZXY(ruv)
ZXY(ruw)
ZXY(rvb)
ZXY(rvc)
ZXY(rvg)
ZXY(rvt)
ZXY(rvv)
ZXY(rvw)
ZXY(rvy)
ZXY(rwv)
ZXY(ryv)

ZXY(sab)
ZXY(saB)
ZXY(sac)
ZXY(sae)
ZXY(saf)
ZXY(sag)
ZXY(sam)
ZXY(san)
ZXY(sap)
ZXY(sar)
ZXY(sat)
ZXY(sau)
ZXY(sav)
ZXY(saw)
ZXY(say)
ZXY(sbb)
ZXY(sbc)
ZXY(sbe)
ZXY(sbf)
ZXY(sbg)
ZXY(sbn)
ZXY(sbp)
ZXY(sbr)
ZXY(sbt)
ZXY(sbu)
ZXY(sbv)
ZXY(sbw)
ZXY(sby)
ZXY(scb)
ZXY(scc)
ZXY(sce)
ZXY(scf)
ZXY(scg)
ZXY(scn)
ZXY(scp)
ZXY(scr)
ZXY(sct)
ZXY(scu)
ZXY(scv)
ZXY(scw)
ZXY(scy)
ZXY(sdb)
ZXY(sdc)
ZXY(sde)
ZXY(sdf)
ZXY(sdg)
ZXY(sdn)
ZXY(sdp)
ZXY(sdr)
ZXY(sdt)
ZXY(sdu)
ZXY(sdv)
ZXY(sdw)
ZXY(sdy)
ZXY(seb)
ZXY(seB)
ZXY(sec)
ZXY(see)
ZXY(sef)
ZXY(seg)
ZXY(sei)
ZXY(sen)
ZXY(sep)
ZXY(ser)
ZXY(set)
ZXY(seu)
ZXY(sev)
ZXY(sew)
ZXY(sey)
ZXY(sfb)
ZXY(sfB)
ZXY(sfc)
ZXY(sfe)
ZXY(sff)
ZXY(sfg)
ZXY(sfn)
ZXY(sfp)
ZXY(sfr)
ZXY(sft)
ZXY(sfu)
ZXY(sfv)
ZXY(sfw)
ZXY(sfy)
ZXY(sgb)
ZXY(sge)
ZXY(sgf)
ZXY(sgg)
ZXY(sgn)
ZXY(sgp)
ZXY(sgr)
ZXY(sgt)
ZXY(sgu)
ZXY(sgv)
ZXY(sgw)
ZXY(sgy)

ZXY(sib)
ZXY(sic)
ZXY(sie)
ZXY(sif)
ZXY(sig)
ZXY(sii)
ZXY(sip)
ZXY(sir)
ZXY(sit)
ZXY(siu)
ZXY(siv)
ZXY(siw)
ZXY(siy)

ZXY(smb)
ZXY(smc)
ZXY(sme)
ZXY(smf)
ZXY(smg)
ZXY(smn)
ZXY(smp)
ZXY(smr)
ZXY(smt)
ZXY(smu)
ZXY(smv)
ZXY(smw)
ZXY(smy)
ZXY(snb)
ZXY(snc)
ZXY(sne)
ZXY(snf)
ZXY(sng)
ZXY(snm)
ZXY(snn)
ZXY(snp)
ZXY(snr)
ZXY(snt)
ZXY(snu)
ZXY(snv)
ZXY(snw)
ZXY(sny)

ZXY(spa)
ZXY(spb)
ZXY(spc)
ZXY(spe)
ZXY(spf)
ZXY(spg)
ZXY(spn)
ZXY(spp)
ZXY(spr)
ZXY(spt)
ZXY(spu)
ZXY(spv)
ZXY(spw)
ZXY(spy)

ZXY(srb)
ZXY(src)
ZXY(sre)
ZXY(srf)
ZXY(srg)
ZXY(srp)
ZXY(srr)
ZXY(srt)
ZXY(sru)
ZXY(srv)
ZXY(srw)
ZXY(sry)

ZXY(ssb)
ZXY(ssc)
ZXY(sse)
ZXY(ssg)
ZXY(ssu)
ZXY(ssv)
ZXY(ssw)
ZXY(ste)
ZXY(stp)
ZXY(stt)
ZXY(stv)
ZXY(sub)
ZXY(suc)
ZXY(sue)
ZXY(suf)
ZXY(sug)
ZXY(sun)
ZXY(sup)
ZXY(sur)
ZXY(sut)
ZXY(suu)
ZXY(suv)
ZXY(suw)
ZXY(suy)
ZXY(svb)
ZXY(svc)
ZXY(sve)
ZXY(svf)
ZXY(svg)
ZXY(svp)
ZXY(svr)
ZXY(svt)
ZXY(svu)
ZXY(svv)
ZXY(svw)
ZXY(svy)
ZXY(swb)
ZXY(swc)
ZXY(swe)
ZXY(swf)
ZXY(swg)
ZXY(swp)
ZXY(swr)
ZXY(swt)
ZXY(swu)
ZXY(swv)
ZXY(sww)
ZXY(swy)
ZXY(syb)
ZXY(syc)
ZXY(sye)
ZXY(syf)
ZXY(syg)
ZXY(syi)
ZXY(syn)
ZXY(syp)
ZXY(syr)
ZXY(syt)
ZXY(syu)
ZXY(syv)
ZXY(syw)
ZXY(syy)


ZXY(tav)
ZXY(tbv)
ZXY(tcv)
ZXY(tdv)
ZXY(tev)
ZXY(tfv)
ZXY(tgv)
ZXY(tiy)
ZXY(tmv)
ZXY(tnv)
ZXY(tpv)
ZXY(trv)
ZXY(tsv)
ZXY(ttv)
ZXY(tuv)
ZXY(tvv)
ZXY(twv)
ZXY(tyv)

list<Mode> l{a, aa, t, tt, i, ii, s, ss, D, DD, b, bb, g, gg, r, rr, m, mm,
    ab, aD, ai, as, at, ba, bD, bi, bs, bt, Da, Db, Di, Ds, Dt, ia, ib, iD, is, it, sa, sb, sD, si, st, ta, tb, tD, ti, ts,
    ag, am, ar, bg, bm, br, Dg, Dm, Dr, ig, im, ir, sg, sm, sr, tg, tm, tr,
    ga, gb, gD, gi, gm, gr, gs, gt, ma, mb, mD, mg, mi, mr, ms, mt, ra, rb, rD, rg, ri, rm, rs, rt,
    ac, bc, Dc, gc, ic, mc, rc, sc, tc,
    af, bf, Df, gf, ifZ, mf, rf, sf, tf,
    ad, bd, Dd, id, gd, md, rd, sd, td,
    au, bu, Du, gu, iu, mu, ru, su, tu,
    ay, by, Dy, gy, iy, my, ry, sy, ty,
    ae, be, De, ge, ie, me, re, se, te,
    aw, bw, Dw, gw, iw, mw, rw, sw, tw,
    av, bv, Dv, gv, iv, mv, rv, sv, tv,
    y,
    yy, ya, yb, yc, yd, yD, ye, yf, yg, yi, ym, yr, ys, yu, yv, yw,
    ap, bp, Dp, gp, ip, mp, rp, sp, tp, yp,
    an, bn, Dn, gn, in, mn, rn, sn, tn, yn,
    ee, ea, eb, ec, ed, eD, ef, eg, ei, em, en, ep, er, es, et, eu, ev, ew, ey,
    wa, wb, wc, wD, we, wf, wg, wi, wm, wn, wp, wr, ws, wt, wu, wv, wy,
    aaf,
    aau,
    aav,
    aay,
    abb,
    aby,
    afv,
    agb,
    age,
    agg,
    agp,
    agu,
    agw,
    agy,
    bbb,
    bbf,
    bbg,
    bbm,
    bbp,
    bbv,
    bbw,
    bby,
    bDv,
    bib,
    big,
    bii,
    bip,
    biv,
    bfd,
    Dag,
    Dbb,
    Dbf,
    Dbg,
    Dbw,
    DDy,
    Dgb,
    Dgf,
    Dgt,
    Dgu,
    Dgw,
    Dmb,
    Dmc,
    Dmf,
    Dmp,
    Dmt,
    Dmu,
    Dmw,
    Dmy,
    Dsb,
    Dsf,
    Dsg,
    Dsw,
    Dsy,
    eav,
    ebv,
    eDv,
    eiv,
    esv,
    etv,
    gab,
    gac,
    gae,
    gaf,
    gag,
    gap,
    gar,
    gat,
    gau,
    gav,
    gaw,
    gay,
    gbb,
    gbe,
    gbg,
    gbp,
    gbr,
    gbt,
    gbu,
    gbv,
    gbw,
    gby,
    gib,
    gic,
    gie,
    gif,
    gig,
    gin,
    gip,
    gir,
    git,
    giu,
    giv,
    giw,
    giy,
    gpb,
    gpe,
    gpf,
    gmc,
    gmv,
    gsb,
    gsf,
    gsg,
    gsp,
    gst,
    gsu,
    gsw,
    guu,
    guv,
    gyp,
    ibv,
    iDc,
    mvf,
    rab,
    rac,
    rae,
    raf,
    rag,
    rap,
    rar,
    rat,
    rau,
    rav,
    raw,
    ray,
    rbb,
    rbc,
    rbf,
    rbg,
    rbi,
    rbp,
    rbr,
    rbt,
    rbu,
    rbv,
    rbw,
    rby,
    rDb,
    rDe,
    rDg,
    rDt,
    rDu,
    rDv,
    rDw,
    rDy,
    rfb,
    rfe,
    rff,
    rfg,
    rfp,
    rft,
    rfv,
    rfw,
    rge,
    rgg,
    rgt,
    rib,
    rig,
    rip,
    rir,
    rit,
    riv,
    riw,
    riy,
    rsb,
    rsc,
    rse,
    rsf,
    rsg,
    rsp,
    rsr,
    rst,
    rsu,
    rsv,
    rsw,
    rsy,
    rtb,
    rtc,
    rtf,
    rtg,
    rtr,
    rtt,
    rtv,
    rtw,
    rty,
    rug,
    ruv,
    ruw,
    rvb,
    rvc,
    rvg,
    rvt,
    rvv,
    rvw,
    rvy,
    rwv,
    ryv,
    sab,
    saB,
    sac,
    sae,
    saf,
    sag,
    sam,
    san,
    sap,
    sar,
    sat,
    sau,
    sav,
    saw,
    say,
    sbb,
    sbc,
    sbe,
    sbf,
    sbg,
    sbn,
    sbp,
    sbr,
    sbt,
    sbu,
    sbv,
    sbw,
    sby,
    scb,
    scc,
    sce,
    scf,
    scg,
    scn,
    scp,
    scr,
    sct,
    scu,
    scv,
    scw,
    scy,
    sdb,
    sdc,
    sde,
    sdf,
    sdg,
    sdn,
    sdp,
    sdr,
    sdt,
    sdu,
    sdv,
    sdw,
    sdy,
    seb,
    seB,
    sec,
    see,
    sef,
    seg,
    sei,
    sen,
    sep,
    ser,
    set,
    seu,
    sev,
    sew,
    sey,
    sfb,
    sfB,
    sfc,
    sfe,
    sff,
    sfg,
    sfn,
    sfp,
    sfr,
    sft,
    sfu,
    sfv,
    sfw,
    sfy,
    sgb,
    sge,
    sgf,
    sgg,
    sgn,
    sgp,
    sgr,
    sgt,
    sgu,
    sgv,
    sgw,
    sgy,
    sib,
    sic,
    sie,
    sif,
    sig,
    sii,
    sip,
    sir,
    sit,
    siu,
    siv,
    siw,
    siy,
    smb,
    smc,
    sme,
    smf,
    smg,
    smn,
    smp,
    smr,
    smt,
    smu,
    smv,
    smw,
    smy,
    snb,
    snc,
    sne,
    snf,
    sng,
    snm,
    snn,
    snp,
    snr,
    snt,
    snu,
    snv,
    snw,
    sny,
    spa,
    spb,
    spc,
    spe,
    spf,
    spg,
    spn,
    spp,
    spr,
    spt,
    spu,
    spv,
    spw,
    spy,
    srb,
    src,
    sre,
    srf,
    srg,
    srp,
    srr,
    srt,
    sru,
    srv,
    srw,
    sry,
    ssb,
    ssc,
    sse,
    ssg,
    ssu,
    ssv,
    ssw,
    ste,
    stp,
    stt,
    stv,
    sub,
    suc,
    sue,
    suf,
    sug,
    sun,
    sup,
    sur,
    sut,
    suu,
    suv,
    suw,
    suy,
    svb,
    svc,
    sve,
    svf,
    svg,
    svp,
    svr,
    svt,
    svu,
    svv,
    svw,
    svy,
    swb,
    swc,
    swe,
    swf,
    swg,
    swp,
    swr,
    swt,
    swu,
    swv,
    sww,
    swy,
    syb,
    syc,
    sye,
    syf,
    syg,
    syi,
    syn,
    syp,
    syr,
    syt,
    syu,
    syv,
    syw,
    syy,
    tav,
    tbv,
    tcv,
    tdv,
    tev,
    tfv,
    tgv,
    tiy,
    tmv,
    tnv,
    tpv,
    trv,
    tsv,
    ttv,
    tuv,
    tvv,
    twv,
    tyv,
};

QYX(fuf)
list<Mode> Ql{
    fuf,
};


static gboolean modify_key_feed(GdkEventKey *event, keybind_info *info, const std::map<int, const char *>&table) {
    if (info->config.modify_other_keys) {
        unsigned int keyVal = gdk_keyval_to_lower(event->keyval);
        auto entry = table.find((int)keyVal);

        if (entry != table.end()) {
            vte_terminal_feed_child(info->vte, entry->second, -1);
            return TRUE;
        }
    }
    return FALSE;
}

static void modifyKeyFeed(GdkEventKey *event, keybind_info *info, const std::map<int, const char *>&table) {
    unsigned int keyVal = gdk_keyval_to_lower(event->keyval);
    auto entry = table.find((int)keyVal);

    if (entry != table.end()) {
        vte_terminal_feed_child(info->vte, entry->second, -1);
    }
}

static gboolean redirectKey(unsigned int keyVal, keybind_info *info, const std::map<int, const char *>&ƌ) {
    auto entry = ƌ.find(keyVal);
    if (entry != ƌ.end()) {
        vte_terminal_feed_child(info->vte, entry->second, -1);
        return TRUE;
    }
    return FALSE;
}

static void launch_in_directory(VteTerminal *vte) {
    const char *uri = vte_terminal_get_current_directory_uri(vte);
    if (!uri) {
        g_printerr("no directory uri set\n");
        return;
    }
    auto dir = make_unique(g_filename_from_uri(uri, nullptr, nullptr), g_free);
    char term[] = "ztermite"; // maybe this should be argv[0]
    char *cmd[] = {term, nullptr};
    g_spawn_async(dir.get(), cmd, nullptr, G_SPAWN_SEARCH_PATH, nullptr, nullptr, nullptr, nullptr);
}


/* {{{ CALLBACKS */
void window_title_cb(VteTerminal *vte, gboolean *dynamic_title) {
    const char *const title = *dynamic_title ? vte_terminal_get_window_title(vte) : nullptr;
    gtk_window_set_title(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(vte))), title ? title : "ztermite");
    if(title) {
        winTitle = string(title);
    }
}

static void reset_font_scale(VteTerminal *vte, gdouble scale) {
    vte_terminal_set_font_scale(vte, scale);

    string fontStrFile = "";
    fontFile.open("/home/simon/afc/fcg/fcg");
    fontFile >> fontStrFile;
    fontFile.close();

    fontStrFile = fontStrFile + " 11";
    char tab2[1024];
    strcpy(tab2, fontStrFile.c_str());

    PangoFontDescription *font = pango_font_description_from_string(tab2);
    vte_terminal_set_font(vte, font);
}

static void increase_font_scale(VteTerminal *vte) {
    gdouble scale = vte_terminal_get_font_scale(vte);

    for (auto it = zoom_factors.begin(); it != zoom_factors.end(); ++it) {
        if ((*it - scale) > 1e-6) {
            vte_terminal_set_font_scale(vte, *it);
            return;
        }
    }
}

static void decrease_font_scale(VteTerminal *vte) {
    gdouble scale = vte_terminal_get_font_scale(vte);

    for (auto it = zoom_factors.rbegin(); it != zoom_factors.rend(); ++it) {
        if ((scale - *it) > 1e-6) {
            vte_terminal_set_font_scale(vte, *it);
            return;
        }
    }
}

static void pushKey(char cs) {
    GdkRGBA color;
    color.red = 0.247;
    color.blue = 0.1;
    color.green = 0.247;
    color.alpha = 1;

    keySeq.push_back(cs);
    /* if (keySeq.size() >= 3) { */
    /*     keySeq.pop_front(); */
    /* } */
    if (keySeq.size() >= 4) {
        color.red = 0.6;
        keySeq.pop_front();
    }
    vte_terminal_set_color_background(VTEGLOBE, &color);
    override_background_color(GTK_WIDGET(WINGLOBE), &color);
}

static void qPushKey(char cs) {
    GdkRGBA color;
    color.red = 0.247;
    color.blue = 0.1;
    color.green = 0.247;
    color.alpha = 1;

    qKeySeq.push_back(cs);
    /* if (keySeq.size() >= 3) { */
    /*     keySeq.pop_front(); */
    /* } */
    if (qKeySeq.size() >= 4) {
        color.red = 0.6;
        qKeySeq.pop_front();
    }
    vte_terminal_set_color_background(VTEGLOBE, &color);
    override_background_color(GTK_WIDGET(WINGLOBE), &color);
}
static void actQ() {
    GdkRGBA color;
    color.red = 0.247;
    color.blue = 0.1;
    color.green = 0.247;
    color.alpha = 1;
    vte_terminal_set_color_background(VTEGLOBE, &color);
    override_background_color(GTK_WIDGET(WINGLOBE), &color);
}

static void clearKey() {
    GdkRGBA color;
    color.red = 0.247;
    color.blue = 0.247;
    color.green = 0.247;
    color.alpha = 1;
    vte_terminal_set_color_background(VTEGLOBE, &color);
    override_background_color(GTK_WIDGET(WINGLOBE), &color);

    keySeq = list<char>();
} 

static void qClearKey() {
    GdkRGBA color;
    color.red = 0.247;
    color.blue = 0.247;
    color.green = 0.247;
    color.alpha = 1;
    vte_terminal_set_color_background(VTEGLOBE, &color);
    override_background_color(GTK_WIDGET(WINGLOBE), &color);

    qKeySeq = list<char>();
    useQ = 0;
} 

#include <functional>

template<typename T, typename Pred>
T const &clamp(T const &val, T const &lo, T const &hi, Pred p) {
  return p(val, lo) ? lo : p(hi, val) ? hi : val;
}

template<typename T>
T const &clamp(const T &val, T const &lo, T const &hi) {
  return clamp(val, lo, hi, std::less<T>());
}

static long first_row(VteTerminal *vte) {
    GtkAdjustment *adjust = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(vte));
    return (long)gtk_adjustment_get_lower(adjust);
}

static long last_row(VteTerminal *vte) {
    GtkAdjustment *adjust = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(vte));
    return (long)gtk_adjustment_get_upper(adjust) - 1;

}
static void update_scroll(VteTerminal *vte) {
    GtkAdjustment *adjust = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(vte));
    const double scroll_row = gtk_adjustment_get_value(adjust);
    const long n_rows = vte_terminal_get_row_count(vte);
    long cursor_row;
    vte_terminal_get_cursor_position(vte, nullptr, &cursor_row);

    if ( (double)cursor_row < scroll_row) {
        gtk_adjustment_set_value(adjust, (double)cursor_row);
    } else if (cursor_row - n_rows >= (long)scroll_row) {
        gtk_adjustment_set_value(adjust, (double)(cursor_row - n_rows + 1));
    }
}

static void update_scroll2(VteTerminal *vte) {
    GtkAdjustment *adjust = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(vte));
    const double scroll_row = gtk_adjustment_get_value(adjust);
    const long n_rows = vte_terminal_get_row_count(vte);
    long cursor_row;
    vte_terminal_get_cursor_position(vte, nullptr, &cursor_row);

    gtk_adjustment_set_value(adjust, (double)(cursor_row - n_rows + 1));
    /* if ( (double)cursor_row < scroll_row) { */
    /*     gtk_adjustment_set_value(adjust, (double)cursor_row); */
    /* } else if (cursor_row - n_rows >= (long)scroll_row) { */
    /*     gtk_adjustment_set_value(adjust, (double)(cursor_row - n_rows + 1)); */
    /* } */
}

static void move(VteTerminal *vte, long col, long row) {
    if (pausePty == 0) {
        GdkRGBA color;
        color.red = 0.247;
        color.blue = 0.247;
        color.green = 0.1;
        color.alpha = 1;
        vte_terminal_set_color_background(VTEGLOBE, &color);
        override_background_color(GTK_WIDGET(WINGLOBE), &color);

        vte_terminal_disconnect_pty_read(vte);
        long cursor_col2, cursor_row2;
        vte_terminal_get_cursor_position(vte, &cursor_col2, &cursor_row2);
        originRow = cursor_row2;
        originCol = cursor_col2;
        pausePty = 1;
    }

    const long end_col = vte_terminal_get_column_count(vte) - 1;

    long cursor_col, cursor_row;
    vte_terminal_get_cursor_position(vte, &cursor_col, &cursor_row);
    vte_terminal_set_cursor_position(vte,
                                     clamp(cursor_col + col, 0l, end_col),
                                     clamp(cursor_row + row, first_row(vte), last_row(vte)));
    /* vte_terminal_set_cursor_position(vte, */
    /*                                  clamp(cursor_col + col, 0l, end_col), */
    /*                                  clamp(cursor_row + row, first_row(vte), last_row(vte))); */

    update_scroll2(vte);
}


gboolean key_press_cb(VteTerminal *vte, GdkEventKey *event, keybind_info *info) {
    const guint modifiers = event->state & gtk_accelerator_get_default_mod_mask();

    activityFile << "1";
    activityFile.flush();

    if(modifiers == GDK_MOD1_MASK) {
        switch (gdk_keyval_to_lower(event->keyval)) {
            case GDK_KEY_comma:
                return FALSE;
                break;
            case GDK_KEY_v:
                vte_terminal_paste_clipboard(vte);
                return TRUE;
        }
    }

    if ((modifiers == GDK_CONTROL_MASK) && (pausePty == 1)) {
        switch (gdk_keyval_to_lower(event->keyval)) {
            case GDK_KEY_q:
                move(vte, 0, -(vte_terminal_get_row_count(vte) / 2));
                return TRUE;
            case GDK_KEY_a:
                move(vte, 0, (vte_terminal_get_row_count(vte) / 2));
                return TRUE;
            case GDK_KEY_u:
                move(vte, 0, -(vte_terminal_get_row_count(vte) / 2));
                return TRUE;
            case GDK_KEY_d:
                move(vte, 0, (vte_terminal_get_row_count(vte) / 2));
                return TRUE;
            case GDK_KEY_z:
                if (pausePty == 1) {
                    GdkRGBA color;
                    color.red = 0.247;
                    color.blue = 0.247;
                    color.green = 0.247;
                    color.alpha = 1;
                    vte_terminal_set_color_background(VTEGLOBE, &color);
                    override_background_color(GTK_WIDGET(WINGLOBE), &color);

                    vte_terminal_set_cursor_position(vte, originCol, originRow);
                    update_scroll(vte);
                    vte_terminal_connect_pty_read(vte);
                }
                pausePty = 0;
                return TRUE;
            default:
                return TRUE;
            }
    }
    if (pausePty == 1) {
        switch (gdk_keyval_to_lower(event->keyval)) {
            case GDK_KEY_q:
                if (pausePty == 1) {
                    GdkRGBA color;
                    color.red = 0.247;
                    color.blue = 0.247;
                    color.green = 0.247;
                    color.alpha = 1;
                    vte_terminal_set_color_background(VTEGLOBE, &color);
                    override_background_color(GTK_WIDGET(WINGLOBE), &color);

                    vte_terminal_set_cursor_position(vte, originCol, originRow);
                    update_scroll(vte);
                    vte_terminal_connect_pty_read(vte);
                }
                pausePty = 0;
                return TRUE;
        }
        return TRUE;
    }

    if (modifiers == (GDK_CONTROL_MASK|GDK_SHIFT_MASK)) {
        switch (gdk_keyval_to_lower(event->keyval)) {
            case GDK_KEY_plus:
                increase_font_scale(vte);
                return TRUE;
            case GDK_KEY_equal:
                reset_font_scale(vte, info->config.font_scale);
                return TRUE;
            case GDK_KEY_t:
                launch_in_directory(vte);
                return TRUE;
            case GDK_KEY_c:
                vte_terminal_copy_clipboard(vte);
                return TRUE;
            case GDK_KEY_v:
                vte_terminal_paste_clipboard(vte);
                return TRUE;
            case GDK_KEY_l:
                vte_terminal_reset(vte, TRUE, TRUE);
                return TRUE;
            default:
                if (modify_key_feed(event, info, modify_table))
                    return TRUE;
        }
    } else if ((modifiers == (GDK_CONTROL_MASK|GDK_MOD1_MASK)) ||
               (modifiers == (GDK_CONTROL_MASK|GDK_MOD1_MASK|GDK_SHIFT_MASK))) {
        switch (gdk_keyval_to_lower(event->keyval)) {
            case GDK_KEY_semicolon:
                vte_terminal_feed_child(info->vte, "\x1B[18~", -1); //Send F7
                return TRUE;
            case GDK_KEY_9:
                clearKey();
                return TRUE;
        }
        if (modify_key_feed(event, info, modify_meta_table))
            return TRUE;
    } else if (modifiers == GDK_CONTROL_MASK) {
        switch (gdk_keyval_to_lower(event->keyval)) {
            case GDK_KEY_plus:
                increase_font_scale(vte);
                return TRUE;
            //case GDK_KEY_minus:
                //decrease_font_scale(vte);
                //return TRUE;
            case GDK_KEY_apostrophe:
                pushKey('a');
                return TRUE;
            case GDK_KEY_equal:
                reset_font_scale(vte, info->config.font_scale);
                return TRUE;
            case GDK_KEY_w:
                pushKey('w');
                return TRUE;
            case GDK_KEY_e:
                pushKey('e');
                return TRUE;
            case GDK_KEY_r:
                pushKey('r');
                return TRUE;
            case GDK_KEY_t:
                pushKey('t');
                return TRUE;
            case GDK_KEY_y:
                pushKey('y');
                return TRUE;
            case GDK_KEY_u:
                if (keySeq.size() == 0) {
                    if(winTitle.find("ꖦѴ") != string::npos || winTitle.find("ࢴѴ") != string::npos) {
                        return FALSE;
                    } else {
                        move(vte, 0, -(vte_terminal_get_row_count(vte) / 2));
                        return TRUE;
                    }
                }
                pushKey('u');
                return TRUE;
            case GDK_KEY_p:
                if (keySeq.size() == 0) {
                    return FALSE;
                }
                pushKey('p');
                return TRUE;
            case GDK_KEY_i:
                pushKey('i');
                return TRUE;
            case GDK_KEY_period:
                if(winTitle.find("ꖦѴ") != string::npos) {
                    vte_terminal_feed_child(info->vte, "\x1B[28~", -1); //Send F15
                }
                pushKey('D');
                return TRUE;
            case GDK_KEY_slash:
                pushKey('s');
                return TRUE;
            case GDK_KEY_d:
                if (keySeq.size() == 0) {
                    return FALSE;
                }
                pushKey('d');
                return TRUE;
            case GDK_KEY_f:
                if (keySeq.size() == 0) {
                    return FALSE;
                }
                pushKey('f');
                return TRUE;
            case GDK_KEY_g:
                pushKey('g');
                return TRUE;
            case GDK_KEY_bracketright:
                if(winTitle.find("ꖦѴ") != string::npos) {
                    vte_terminal_feed_child(info->vte, "\x1B[17~", -1); //Send F6
                }
                pushKey('b');
                return TRUE;
            case GDK_KEY_c:
                if (keySeq.size() == 0) {
                    return FALSE;
                }
                pushKey('c');
                return TRUE;
            case GDK_KEY_v:
                if (keySeq.size() == 0) {
                    return FALSE;
                }
                pushKey('v');
                return TRUE;
            case GDK_KEY_b:
                if (keySeq.size() == 0) {
                    return FALSE;
                }
                pushKey('B');
                return TRUE;
            case GDK_KEY_m:
                modifyKeyFeed(event, info, modifyControlTable);
                return TRUE;
            case GDK_KEY_n:
                if (keySeq.size() == 0) {
                    return FALSE;
                }
                pushKey('n');
                return TRUE;
            case GDK_KEY_semicolon:
                modifyKeyFeed(event, info, modifyControlTable);
                return TRUE;
            case GDK_KEY_comma:
                if(winTitle.find("ꖦѴ") != string::npos) {
                    vte_terminal_feed_child(info->vte, "\x1BOP", -1); //Send F1
                }
                pushKey('m');
                return TRUE;
            case GDK_KEY_Return:
                modifyKeyFeed(event, info, modifyControlTable);
                return TRUE;
            /* case GDK_KEY_v: */
            /*     pushKey('v'); */
            /*     return TRUE; */
            default:
                if (modify_key_feed(event, info, modify_table))
                    return TRUE;
        }
    }

    unsigned int keyVal = gdk_keyval_to_lower(event->keyval);
    if(keyVal == GDK_KEY_Shift_L)
        return FALSE;
    if(keyVal == GDK_KEY_Shift_R)
        return FALSE;
    if(keyVal == GDK_KEY_Super_R) {
        printf("the ALT R id is %u\n", 233);
        if(useQ == 0) {
            useQ = 1;
            actQ();
        } else {
            qClearKey();
        }
        return FALSE;
    }

    if(useQ == 1) {
        if(qKeySeq.size() == 3) {

            for(auto& i: Ql) {
                if (qKeySeq == i.seq) {

                    qClearKey();

                    auto ƌ = modifiers != GDK_SHIFT_MASK ? i.ƌ : i.shiftƌ;
                    if(redirectKey(keyVal, info, ƌ)) {
                        return TRUE;
                    } else {
                        return TRUE;
                    }
                }
            }

            return TRUE;
        } else {
            switch (gdk_keyval_to_lower(event->keyval)) {
                case GDK_KEY_w:
                    qPushKey('w');
                    break;
                case GDK_KEY_e:
                    qPushKey('e');
                    break;
                case GDK_KEY_r:
                    qPushKey('r');
                    break;
                case GDK_KEY_t:
                    qPushKey('t');
                    break;
                case GDK_KEY_y:
                    qPushKey('y');
                    break;
                case GDK_KEY_u:
                    qPushKey('u');
                    break;
                case GDK_KEY_i:
                    qPushKey('i');
                    break;
                case GDK_KEY_s:
                    qPushKey('s');
                    break;
                case GDK_KEY_f:
                    qPushKey('f');
                    break;
                case GDK_KEY_v:
                    qPushKey('v');
                    break;
                case GDK_KEY_b:
                    qPushKey('b');
                    break;
                case GDK_KEY_n:
                    qPushKey('n');
                    break;
                case GDK_KEY_m:
                    qPushKey('m');
                    break;
                default:
                    break;
            }
            return TRUE;
        }
    }

    auto entry = keyƌ.find((int)keyVal);
    auto key = entry != keyƌ.end() ? entry->second : "";

    for(auto& i: l) {
        if (keySeq == i.seq) {
            clearKey();
            auto ƌ = modifiers != GDK_SHIFT_MASK ? i.ƌ : i.shiftƌ;
            if(redirectKey(keyVal, info, ƌ)) {
                return TRUE;
            } else {
                return FALSE;
            }
        }
    }

    if(mode == "normal") {
        if(keyVal == GDK_KEY_comma) {
            mode = "punc";
            //vte_terminal_feed_child(info->vte, "\x1B[20~", -1); //Send F9
            return TRUE;
        } else if(keyVal == GDK_KEY_semicolon) {
            mode = "number";
            return TRUE;
        }
    } else if(mode == "punc")  {
        auto ƌ = modifiers != GDK_SHIFT_MASK ? puncƌ : puncShiftƌ;
        mode = "normal";
        if(redirectKey(keyVal, info, ƌ)) {
            return TRUE;
        } else {
            vte_terminal_feed_child(info->vte, ",", -1);
            return FALSE;
        }
    } else if(mode == "number") {
        auto ƌ = modifiers != GDK_SHIFT_MASK ? numberƌ : numberShiftƌ;
        mode = "normal";
        if(redirectKey(keyVal, info, ƌ)) {
            return TRUE;
        } else {
            vte_terminal_feed_child(info->vte, ";", -1);
            return FALSE;
        }
    }

    if(gdk_keyval_to_lower(event->keyval) == GDK_KEY_Tab) {
        if (modify_key_feed(event, info, modify_table))
            return TRUE;
    }
    return FALSE;
}

gboolean position_overlay_cb(GtkBin *overlay, GtkWidget *widget, GdkRectangle *alloc) {
    GtkWidget *vte = gtk_bin_get_child(overlay);

    const int width  = gtk_widget_get_allocated_width(vte);
    const int height = gtk_widget_get_allocated_height(vte);

    GtkRequisition req;
    gtk_widget_get_preferred_size(widget, nullptr, &req);

    alloc->x = width - req.width - 40;
    alloc->y = 0;
    alloc->width  = std::min(width, req.width);
    alloc->height = std::min(height, req.height);

    return TRUE;
}


/* {{{ CONFIG LOADING */
template<typename T>
maybe<T> get_config(T (*get)(GKeyFile *, const char *, const char *, GError **),
                    GKeyFile *config, const char *group, const char *key) {
    GError *error = nullptr;
    maybe<T> value = get(config, group, key, &error);
    if (error) {
        g_error_free(error);
        return {};
    }
    return value;
}

auto get_config_integer(std::bind(get_config<int>, g_key_file_get_integer, _1, _2, _3));
auto get_config_string(std::bind(get_config<char *>, g_key_file_get_string, _1, _2, _3));
auto get_config_double(std::bind(get_config<double>, g_key_file_get_double, _1, _2, _3));

static maybe<GdkRGBA> get_config_color(GKeyFile *config, const char *section, const char *key) {
    if (auto s = get_config_string(config, section, key)) {
        GdkRGBA color;
        if (gdk_rgba_parse(&color, *s)) {
            g_free(*s);
            return color;
        }
        g_printerr("invalid color string: %s\n", *s);
        g_free(*s);
    }
    return {};
}

static void load_theme(GtkWindow *window, VteTerminal *vte, GKeyFile *config) {
    std::array<GdkRGBA, 256> palette;
    char color_key[] = "color000";

    for (unsigned i = 0; i < palette.size(); i++) {
        snprintf(color_key, sizeof(color_key), "color%u", i);
        if (auto color = get_config_color(config, "colors", color_key)) {
            palette[i] = *color;
        } else if (i < 16) {
            palette[i].blue = (((i & 4) ? 0xc000 : 0) + (i > 7 ? 0x3fff: 0)) / 65535.0;
            palette[i].green = (((i & 2) ? 0xc000 : 0) + (i > 7 ? 0x3fff : 0)) / 65535.0;
            palette[i].red = (((i & 1) ? 0xc000 : 0) + (i > 7 ? 0x3fff : 0)) / 65535.0;
            palette[i].alpha = 0;
        } else if (i < 232) {
            const unsigned j = i - 16;
            const unsigned r = j / 36, g = (j / 6) % 6, b = j % 6;
            const unsigned red =   (r == 0) ? 0 : r * 40 + 55;
            const unsigned green = (g == 0) ? 0 : g * 40 + 55;
            const unsigned blue =  (b == 0) ? 0 : b * 40 + 55;
            palette[i].red   = (red | red << 8) / 65535.0;
            palette[i].green = (green | green << 8) / 65535.0;
            palette[i].blue  = (blue | blue << 8) / 65535.0;
            palette[i].alpha = 0;
        } else if (i < 256) {
            const unsigned shade = 8 + (i - 232) * 10;
            palette[i].red = palette[i].green = palette[i].blue = (shade | shade << 8) / 65535.0;
            palette[i].alpha = 0;
        }
    }

    vte_terminal_set_colors(vte, nullptr, nullptr, palette.data(), palette.size());
    if (auto color = get_config_color(config, "colors", "foreground")) {
        vte_terminal_set_color_foreground(vte, &*color);
        vte_terminal_set_color_bold(vte, &*color);
    }
    if (auto color = get_config_color(config, "colors", "foreground_bold")) {
        vte_terminal_set_color_bold(vte, &*color);
    }
    if (auto color = get_config_color(config, "colors", "background")) {
        vte_terminal_set_color_background(vte, &*color);
        override_background_color(GTK_WIDGET(window), &*color);
    }
    if (auto color = get_config_color(config, "colors", "cursor")) {
        vte_terminal_set_color_cursor(vte, &*color);
    }
    if (auto color = get_config_color(config, "colors", "cursor_foreground")) {
        //vte_terminal_set_color_cursor_foreground(vte, &*color);
    }
    if (auto color = get_config_color(config, "colors", "highlight")) {
        vte_terminal_set_color_highlight(vte, &*color);
    }

}

static void load_config(GtkWindow *window, VteTerminal *vte, config_info *info, char **geometry, char **icon) {
    const std::string default_path = "/ztermite/config";
    GKeyFile *config = g_key_file_new();
    GError *error = nullptr;

    const std::string defPath = "/home/simon/.config/ztermite/config";

    gboolean loaded = FALSE;

    if (info->config_file) {
        loaded = g_key_file_load_from_file(config,
                                           info->config_file,
                                           G_KEY_FILE_NONE, &error);
        if (!loaded)
            g_printerr("%s parsing failed: %s\n", info->config_file,
                       error->message);
    }

    if (!loaded) {
        /* loaded = g_key_file_load_from_file(config, (g_get_user_config_dir() + default_path).c_str(), G_KEY_FILE_NONE, &error); */
        loaded = g_key_file_load_from_file(config, defPath.c_str(), G_KEY_FILE_NONE, &error);
        if (!loaded)
            g_printerr("%s parsing failed: %s\n", (g_get_user_config_dir() + default_path).c_str(),
                       error->message);
    }

    for (const char *const *dir = g_get_system_config_dirs();
         !loaded && *dir; dir++) {
        loaded = g_key_file_load_from_file(config, (*dir + default_path).c_str(),
                                           G_KEY_FILE_NONE, &error);
        if (!loaded)
            g_printerr("%s parsing failed: %s\n", (*dir + default_path).c_str(),
                       error->message);
    }

    if (loaded) {
        set_config(window, vte, info, geometry, icon, config);
    }
    g_key_file_free(config);
}

static void set_config(GtkWindow *window, VteTerminal *vte,
                       config_info *info, char **geometry, char **icon,
                       GKeyFile *config) {
    if (geometry) {
        if (auto s = get_config_string(config, "options", "geometry")) {
            *geometry = *s;
        }
    }

    auto cfg_bool = [config](const char *key, gboolean value) {
        return get_config<gboolean>(g_key_file_get_boolean,
                                    config, "options", key).get_value_or(value);
    };

    vte_terminal_set_audible_bell(vte, cfg_bool("audible_bell", FALSE));
    vte_terminal_set_mouse_autohide(vte, cfg_bool("mouse_autohide", FALSE));
    vte_terminal_set_allow_bold(vte, cfg_bool("allow_bold", TRUE));
    vte_terminal_set_scrollback_lines(vte, 100000);
    info->dynamic_title = cfg_bool("dynamic_title", TRUE);
    info->urgent_on_bell = cfg_bool("urgent_on_bell", TRUE);
    info->modify_other_keys = cfg_bool("modify_other_keys", FALSE);
    info->font_scale = vte_terminal_get_font_scale(vte);

    vte_terminal_set_enable_bidi(vte, FALSE);

    g_free(info->browser);
    info->browser = nullptr;

    if (auto s = get_config_string(config, "options", "browser")) {
        info->browser = *s;
    } else {
        info->browser = g_strdup(g_getenv("BROWSER"));
    }

    if (!info->browser) {
        info->browser = g_strdup("xdg-open");
    }

    if (info->tag != -1) {
        vte_terminal_match_remove(vte, info->tag);
        info->tag = -1;
    }

    
    string fontStrFile;
    fontFile.open("/home/simon/afc/fcg/fcg");
    fontFile >> fontStrFile;
    fontFile.close();

    fontStrFile = fontStrFile + " 11";
    char tab2[1024];
    strcpy(tab2, fontStrFile.c_str());

    const char * fontStr = "fpw 11";
    PangoFontDescription *font = pango_font_description_from_string(tab2);
    vte_terminal_set_font(vte, font);
    pango_font_description_free(font);

    if (auto s = get_config_string(config, "options", "cursor_blink")) {
        if (!g_ascii_strcasecmp(*s, "system")) {
            vte_terminal_set_cursor_blink_mode(vte, VTE_CURSOR_BLINK_SYSTEM);
        } else if (!g_ascii_strcasecmp(*s, "on")) {
            vte_terminal_set_cursor_blink_mode(vte, VTE_CURSOR_BLINK_ON);
        } else if (!g_ascii_strcasecmp(*s, "off")) {
            vte_terminal_set_cursor_blink_mode(vte, VTE_CURSOR_BLINK_OFF);
        }
        g_free(*s);
    }

    if (auto s = get_config_string(config, "options", "cursor_shape")) {
        if (!g_ascii_strcasecmp(*s, "block")) {
            vte_terminal_set_cursor_shape(vte, VTE_CURSOR_SHAPE_BLOCK);
        } else if (!g_ascii_strcasecmp(*s, "ibeam")) {
            vte_terminal_set_cursor_shape(vte, VTE_CURSOR_SHAPE_IBEAM);
        } else if (!g_ascii_strcasecmp(*s, "underline")) {
            vte_terminal_set_cursor_shape(vte, VTE_CURSOR_SHAPE_UNDERLINE);
        }
        g_free(*s);
    }

    if (icon) {
        if (auto s = get_config_string(config, "options", "icon_name")) {
            *icon = *s;
        }
    }

    load_theme(window, vte, config);
}/*}}}*/

static void exit_with_status(VteTerminal *, int status) {
    gtk_main_quit();
    exit(WIFEXITED(status) ? WEXITSTATUS(status) : EXIT_FAILURE);
}

static void exit_with_success(VteTerminal *) {
    gtk_main_quit();
    exit(EXIT_SUCCESS);
}

static char *get_user_shell_with_fallback() {
    if (const char *env = g_getenv("SHELL") ) {
        if (!((env != NULL) && (env[0] == '\0')))
            return g_strdup(env);
    }

    if (char *command = vte_get_user_shell()) {
        if (!((command != NULL) && (command[0] == '\0')))
           return command;
    }

    return g_strdup("/bin/sh");
}

static void updateCol() {
    GdkRGBA color;
    color.alpha = 1;
    if (pausePty == 0) {
        if (focus == 1) {
            color.red = 0.247;
            color.blue = 0.247;
            color.green = 0.247;
        } else {
            color.red = 0.1;
            color.blue = 0.247;
            color.green = 0.247;
        }
    } else {
        if (focus == 1) {
            color.red = 0.247;
            color.blue = 0.247;
            color.green = 0.1;
        } else {
            color.red = 0.15;
            color.blue = 0.15;
            color.green = 0.05;
        }
    }
    vte_terminal_set_color_background(VTEGLOBE, &color);
    override_background_color(GTK_WIDGET(WINGLOBE), &color);
}

gboolean time_handler(GtkWidget *widget) {
    std::fseek(receiveFile, 0, SEEK_END);
    int pos = std::ftell(receiveFile);
    if (pos > recPos) {
        recPos = pos;
        std::fseek(receiveFile, --pos, SEEK_SET);
        if (std::fgetc(receiveFile) == '1') {
            focus = 1;
        } else {
            focus = 0;
        }
        updateCol();
    }
    return TRUE;
}

int main(int argc, char **argv) {
    activityFile.open("/tmp/ztermiteAct", std::ios::app);

    GError *error = nullptr;
    const char *const term = "xterm-termite";
    char *directory = nullptr;

    gboolean hold = FALSE;
    GOptionContext *context = g_option_context_new(nullptr);
    char *role = nullptr, *geometry = nullptr, *execute = nullptr, *config_file = nullptr;
    char *title = nullptr, *icon = nullptr;
    const GOptionEntry entries[] = {
        {"exec", 'e', 0, G_OPTION_ARG_STRING, &execute, "Command to execute", "COMMAND"},
        {"role", 'r', 0, G_OPTION_ARG_STRING, &role, "The role to use", "ROLE"},
        {"title", 't', 0, G_OPTION_ARG_STRING, &title, "Window title", "TITLE"},
        {"directory", 'd', 0, G_OPTION_ARG_STRING, &directory, "Change to directory", "DIRECTORY"},
        {"geometry", 0, 0, G_OPTION_ARG_STRING, &geometry, "Window geometry", "GEOMETRY"},
        {"hold", 0, 0, G_OPTION_ARG_NONE, &hold, "Remain open after child process exits", nullptr},
        {"config", 'c', 0, G_OPTION_ARG_STRING, &config_file, "Path of config file", "CONFIG"},
        {"icon", 'i', 0, G_OPTION_ARG_STRING, &icon, "Icon", "ICON"},
        {nullptr, 0, 0, G_OPTION_ARG_NONE, nullptr, nullptr, nullptr}
    };
    g_option_context_add_main_entries(context, entries, nullptr);
    g_option_context_add_group(context, gtk_get_option_group(TRUE));

    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_printerr("option parsing failed: %s\n", error->message);
        g_clear_error (&error);
        return EXIT_FAILURE;
    }

    g_option_context_free(context);


    if (directory) {
        if (chdir(directory) == -1) {
            perror("chdir");
            return EXIT_FAILURE;
        }
        g_free(directory);
    }

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    GtkWidget *vte_widget = vte_terminal_new();
    VteTerminal *vte = VTE_TERMINAL(vte_widget);
    VTEGLOBE = vte;
    WINGLOBE = window;

    /* GDK_DRAWABLE_XID(window) */
    /* printf("the X11 id is %u\n", GDK_WINDOW_XID(window)); */
    /* printf("the X11 id is %u\n", GDK_WINDOW_XID(window)); */
    /* printf("the X11 id is %u\n", GDK_WINDOW_XID(window)); */
    /* printf("the X11 id is %u\n", GDK_WINDOW_XID(window)); */
    /* printf("the X11 id is %u\n", GDK_WINDOW_XID(window)); */

    if (role) {
        gtk_window_set_role(GTK_WINDOW(window), role);
        g_free(role);
    }

    char **command_argv;
    char *default_argv[2] = {nullptr, nullptr};

    if (execute) {
        int argcp;
        char **argvp;
        g_shell_parse_argv(execute, &argcp, &argvp, &error);
        if (error) {
            g_printerr("failed to parse command: %s\n", error->message);
            return EXIT_FAILURE;
        }
        command_argv = argvp;
    } else {
        default_argv[0] = get_user_shell_with_fallback();
        command_argv = default_argv;
    }

    keybind_info info {
        GTK_WINDOW(window), vte,
        {
         nullptr, FALSE, FALSE, FALSE, -1, config_file, 0}
    };

    load_config(GTK_WINDOW(window), vte, &info.config, geometry ? nullptr : &geometry, icon ? nullptr : &icon);

    gtk_container_add(GTK_CONTAINER(window), vte_widget);

    if (!hold) {
        g_signal_connect(vte, "child-exited", G_CALLBACK(exit_with_status), nullptr);
    }

    g_signal_connect(window, "destroy", G_CALLBACK(exit_with_success), nullptr);
    g_signal_connect(vte, "key-press-event", G_CALLBACK(key_press_cb), &info);

    g_signal_connect(vte, "window-title-changed", G_CALLBACK(window_title_cb), &info.config.dynamic_title);

    g_timeout_add(50, (GSourceFunc) time_handler, window);

    if(title) {
        gtk_window_set_title(GTK_WINDOW(window), title);
    } else if (execute) {
        gtk_window_set_title(GTK_WINDOW(window), execute);
    } else {
        gtk_window_set_title(GTK_WINDOW(window), "ztermite");
    }

    if (icon) {
        gtk_window_set_icon_name(GTK_WINDOW(window), icon);
        g_free(icon);
    }

    gtk_widget_grab_focus(vte_widget);
    gtk_widget_show_all(window);

    char **env = g_get_environ();

#ifdef GDK_WINDOWING_X11
    if (GDK_IS_X11_SCREEN(gtk_widget_get_screen(window))) {
        GdkWindow *gdk_window = gtk_widget_get_window(window);
        if (!gdk_window) {
            g_printerr("no window\n");
            return EXIT_FAILURE;
        }
        char xid_s[std::numeric_limits<long unsigned>::digits10 + 1];
        snprintf(xid_s, sizeof(xid_s), "%lu", GDK_WINDOW_XID(gdk_window));
        env = g_environ_setenv(env, "WINDOWID", xid_s, TRUE);

        char filename[64];
        sprintf(filename, "/tmp/zterm%u", GDK_WINDOW_XID(gdk_window));
        /* printf("the X11 id is %u\n", GDK_WINDOW_XID(gdk_window)); */
        /* printf("the X11 id is %u\n", GDK_WINDOW_XID(gdk_window)); */
        /* printf("the X11 id is %u\n", GDK_WINDOW_XID(gdk_window)); */
        /* printf("the X11 id is %u\n", GDK_WINDOW_XID(gdk_window)); */
        /* printf("the X11 id is %u\n", GDK_WINDOW_XID(gdk_window)); */
        /* printf("the X11 id is %u\n", GDK_WINDOW_XID(gdk_window)); */
        /* printf("the X11 id is %u\n", GDK_WINDOW_XID(gdk_window)); */
        /* printf("the X11 id is %u\n", GDK_WINDOW_XID(gdk_window)); */
        std::fstream fs;
        /* fs.open(filename, std::ios::out | std::ios::app); */
        int oldval = umask(0011);
        FILE *fp;
        fp = std::fopen(filename, "w");
        std::fclose(fp);

        umask(oldval);

        receiveFile = std::fopen(filename, "r");
    }
#endif

    env = g_environ_setenv(env, "TERM", term, TRUE);

    GPid child_pid;
    if (vte_terminal_spawn_sync(vte, VTE_PTY_DEFAULT, nullptr, command_argv, env,
                                G_SPAWN_SEARCH_PATH, nullptr, nullptr, &child_pid, nullptr,
                                &error)) {
        vte_terminal_watch_child(vte, child_pid);
    } else {
        g_printerr("the command failed to run: %s\n", error->message);
        return EXIT_FAILURE;
    }

    g_strfreev(env);

    gtk_main();
    return EXIT_FAILURE; // child process did not cause termination
}
