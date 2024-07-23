//
// Created by toby on 2023/11/30.
//

#ifndef COMPILER_SETTINGS_H
#define COMPILER_SETTINGS_H

inline struct OptSettings {
    bool force_no_opt;
    bool using_gp;
    bool using_mem2reg;
    bool using_gvn;
    bool using_gcm;
    bool using_constant_folding;
    bool using_arithmetic_folding;
    bool using_block_merging;
    bool using_block_relocation;
    bool using_force_inline;
    bool using_div2mul;
    bool using_inline_printer;
    bool using_array_splitting;
    bool using_inline_global_var;
} opt_settings;

inline void set_optimize_level(int level) {
#define SET(f) opt_settings.f = true;
    opt_settings = {};
    switch (level) {
    case 3: [[fallthrough]];
    case 2:
        SET(using_gp);
        SET(using_gvn);
        SET(using_gcm);
        SET(using_force_inline);
        SET(using_div2mul);
        SET(using_inline_printer);
        SET(using_array_splitting);
        SET(using_inline_global_var);
        [[fallthrough]];
    case 1:
        SET(using_mem2reg);
        SET(using_constant_folding);
        SET(using_arithmetic_folding);
        SET(using_block_merging);
        SET(using_block_relocation);
        break;
    default: SET(force_no_opt); break;
    }
#undef SET
}

#endif  // COMPILER_SETTINGS_H
