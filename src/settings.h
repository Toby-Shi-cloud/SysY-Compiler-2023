//
// Created by toby on 2023/11/30.
//

#ifndef COMPILER_SETTINGS_H
#define COMPILER_SETTINGS_H

inline struct OptSettings {
    bool force_no_opt;
    bool using_gp;
    bool using_mem2reg;
    bool using_lvn;
    bool using_const_fold;
    bool using_block_merging;
    bool using_block_relocation;
} opt_settings;

inline void set_optimize_level(int level) {
#define SET(f) opt_settings.f = true;
    opt_settings = {};
    switch (level) {
        case 3:
            [[fallthrough]];
        case 2:
            SET(using_gp);
            SET(using_lvn);
            [[fallthrough]];
        case 1:
            SET(using_mem2reg);
            SET(using_const_fold);
            SET(using_block_merging);
            SET(using_block_relocation);
            break;
        default:
            SET(force_no_opt);
            break;
    }
#undef SET
}

#endif //COMPILER_SETTINGS_H
