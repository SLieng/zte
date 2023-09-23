#ifndef PUNC_H
#define PUNC_H

using std::map;

static const map<int, const char *> puncƌ = {
    { GDK_KEY_q,          "\\"  },
    { GDK_KEY_w,          "("  },
    { GDK_KEY_e,          ")"  },
    { GDK_KEY_r,          "!"  },
    { GDK_KEY_t,          "|"  },
    /* { GDK_KEY_u,          ""  }, */
    /* { GDK_KEY_i,          ""  }, */
    { GDK_KEY_o,          "@"  },
    /* { GDK_KEY_p,          ""  }, */
    { GDK_KEY_a,          "-"  },
    { GDK_KEY_s,          "*"  },
    { GDK_KEY_d,          "="  },
    { GDK_KEY_f,          "_"  },
    { GDK_KEY_g,          "+"  },
    /* { GDK_KEY_h,          ""  }, */
    /* { GDK_KEY_j,          ""  }, */
    /* { GDK_KEY_k,          ""  }, */
    /* { GDK_KEY_l,          ""  }, */
    { GDK_KEY_z,          "#"  },
    { GDK_KEY_x,          "&"  },
    { GDK_KEY_c,          "^"  },
    { GDK_KEY_v,          "$"  },
    { GDK_KEY_b,          "`"  },
    { GDK_KEY_n,          "%"  },
    /* { GDK_KEY_m,          ""  }, */

    { GDK_KEY_Tab,        "~"  },
    /* { GDK_KEY_bracketleft, ""}, */
    /* { GDK_KEY_bracketright, ""}, */
    /* { GDK_KEY_semicolon,   ""}, */
    /* { GDK_KEY_apostrophe,  ""}, */
    { GDK_KEY_comma,       ","},
    /* { GDK_KEY_period,      ""}, */
    /* { GDK_KEY_slash,       ""}, */
    /* { GDK_KEY_Return,      ""}, */

    { GDK_KEY_Shift_L,    ""  }, //Dummy,
    { GDK_KEY_Shift_R,    ""  }, //Dummy,
};

static const map<int, const char *> puncShiftƌ = {
    { GDK_KEY_q,          "ʕ"  },
    { GDK_KEY_w,          "ᚕ"  },
    /* { GDK_KEY_e,          ""  }, */
    { GDK_KEY_r,          "ਈ"  },
    { GDK_KEY_t,          "ཟ"  },
    /* { GDK_KEY_y,          ""  }, */
    /* { GDK_KEY_u,          ""  }, */
    /* { GDK_KEY_i,          ""  }, */
    { GDK_KEY_o,          "ຜ"  },
    /* { GDK_KEY_p,          ""  }, */
    { GDK_KEY_a,          "ᚚ"  },
    { GDK_KEY_s,          "න"  },
    { GDK_KEY_d,          "ଠ"  },
    { GDK_KEY_f,          "ꔷ"  },
    { GDK_KEY_g,          "ⵜ"  },
    /* { GDK_KEY_h,          ""  }, */
    /* { GDK_KEY_j,          ""  }, */
    /* { GDK_KEY_k,          ""  }, */
    /* { GDK_KEY_l,          ""  }, */
    { GDK_KEY_z,          "ⵌ"  },
    /* { GDK_KEY_x,          ""  }, */
    /* { GDK_KEY_c,          ""  }, */
    { GDK_KEY_v,          "ᡪ"  },
    /* { GDK_KEY_b,          ""  }, */
    { GDK_KEY_n,          "ད"  },
    /* { GDK_KEY_m,          ""  }, */

    /* { GDK_KEY_Tab,         ""  }, */
    /* { GDK_KEY_braceleft, ""}, */
    /* { GDK_KEY_braceright, ""}, */
    /* { GDK_KEY_colon,   ""}, */
    /* { GDK_KEY_quotedbl,  ""}, */
    /* { GDK_KEY_less,       ""}, */
    /* { GDK_KEY_greater,       ""}, */
    { GDK_KEY_question,      "ඳ"},
    /* { GDK_KEY_Return,      ""}, */

    { GDK_KEY_Shift_L,    ""  }, //Dummy,
    { GDK_KEY_Shift_R,    ""  }, //Dummy,
};

#endif
