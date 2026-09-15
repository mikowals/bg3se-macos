/*
 * Tier 0 tests: mod_paths.c — SE directory-name resolution helpers.
 * Covers the display-name/PAK-directory mismatch fixes (#87, #81).
 */

#include "test_harness.h"
#include "mod_paths.h"

TEST(pak_stem_basic) {
    char dir[64];
    ASSERT_TRUE(mod_se_dir_from_pak_name("/a/b/BG3MCM.pak", dir, sizeof(dir)));
    ASSERT_STR_EQ(dir, "BG3MCM");
}

TEST(pak_stem_case_insensitive_ext) {
    char dir[64];
    ASSERT_TRUE(mod_se_dir_from_pak_name("/mods/Trials.PAK", dir, sizeof(dir)));
    ASSERT_STR_EQ(dir, "Trials");
}

TEST(pak_stem_no_directory) {
    char dir[64];
    ASSERT_TRUE(mod_se_dir_from_pak_name("Solo.pak", dir, sizeof(dir)));
    ASSERT_STR_EQ(dir, "Solo");
}

TEST(pak_stem_no_extension) {
    char dir[64];
    ASSERT_TRUE(mod_se_dir_from_pak_name("/a/NoExt", dir, sizeof(dir)));
    ASSERT_STR_EQ(dir, "NoExt");
}

TEST(pak_stem_spaces_preserved) {
    char dir[64];
    ASSERT_TRUE(mod_se_dir_from_pak_name("/m/Trials of Tav - Reloaded.pak",
                                         dir, sizeof(dir)));
    ASSERT_STR_EQ(dir, "Trials of Tav - Reloaded");
}

TEST(pak_stem_rejects_empty) {
    char dir[64];
    ASSERT_FALSE(mod_se_dir_from_pak_name("/a/b/.pak", dir, sizeof(dir)));
    ASSERT_FALSE(mod_se_dir_from_pak_name("/a/b/", dir, sizeof(dir)));
}

TEST(pak_stem_rejects_overflow) {
    char dir[4];
    ASSERT_FALSE(mod_se_dir_from_pak_name("/a/LongName.pak", dir, sizeof(dir)));
}

TEST(entry_dir_match) {
    char dir[64];
    ASSERT_TRUE(mod_entry_se_config_dir(
        "Mods/BG3MCM/ScriptExtender/Config.json", dir, sizeof(dir)));
    ASSERT_STR_EQ(dir, "BG3MCM");
}

TEST(entry_dir_match_with_spaces) {
    char dir[64];
    ASSERT_TRUE(mod_entry_se_config_dir(
        "Mods/Trials of Tav/ScriptExtender/Config.json", dir, sizeof(dir)));
    ASSERT_STR_EQ(dir, "Trials of Tav");
}

TEST(entry_dir_rejects_wrong_prefix) {
    char dir[64];
    ASSERT_FALSE(mod_entry_se_config_dir(
        "Public/X/ScriptExtender/Config.json", dir, sizeof(dir)));
    ASSERT_FALSE(mod_entry_se_config_dir(
        "mods/X/ScriptExtender/Config.json", dir, sizeof(dir)));
}

TEST(entry_dir_rejects_nested_dir) {
    char dir[64];
    ASSERT_FALSE(mod_entry_se_config_dir(
        "Mods/A/B/ScriptExtender/Config.json", dir, sizeof(dir)));
}

TEST(entry_dir_rejects_deeper_path) {
    char dir[64];
    ASSERT_FALSE(mod_entry_se_config_dir(
        "Mods/A/ScriptExtender/Config.json.bak", dir, sizeof(dir)));
    ASSERT_FALSE(mod_entry_se_config_dir(
        "Mods/A/ScriptExtender/Lua/BootstrapServer.lua", dir, sizeof(dir)));
}

TEST(entry_dir_rejects_empty_dir) {
    char dir[64];
    ASSERT_FALSE(mod_entry_se_config_dir(
        "Mods//ScriptExtender/Config.json", dir, sizeof(dir)));
}

TEST(entry_dir_rejects_overflow) {
    char dir[4];
    ASSERT_FALSE(mod_entry_se_config_dir(
        "Mods/LongDirName/ScriptExtender/Config.json", dir, sizeof(dir)));
}


/* ── AUDIT ADDITIONS ─────────────────────────────────────────────── */

/* The two *_rejects_overflow tests above only exercise a GROSS overflow
 * (8- and 11-char names into a 4-byte buffer), so relaxing both bounds checks
 * from `len >= dir_size` to `len > dir_size` — a one-byte NUL write past the
 * end of the caller's buffer — left all 68 tests green. Pin the exact
 * boundary in both directions, with a canary after the buffer. */
TEST(pak_stem_boundary_exact_fit_and_one_over) {
    struct { char dir[4]; char canary[4]; } s;

    memset(&s, 0x5A, sizeof(s));
    ASSERT_TRUE(mod_se_dir_from_pak_name("/m/Abc.pak", s.dir, sizeof(s.dir)));
    ASSERT_STR_EQ(s.dir, "Abc");                 /* 3 chars + NUL == 4 */
    ASSERT_EQ(s.canary[0], (char)0x5A);

    memset(&s, 0x5A, sizeof(s));
    ASSERT_FALSE(mod_se_dir_from_pak_name("/m/Abcd.pak", s.dir, sizeof(s.dir)));
    for (size_t i = 0; i < sizeof(s.canary); i++) {
        ASSERT_EQ(s.canary[i], (char)0x5A);      /* 4 chars must NOT fit */
    }
}

TEST(entry_dir_boundary_exact_fit_and_one_over) {
    struct { char dir[4]; char canary[4]; } s;

    memset(&s, 0x5A, sizeof(s));
    ASSERT_TRUE(mod_entry_se_config_dir(
        "Mods/Abc/ScriptExtender/Config.json", s.dir, sizeof(s.dir)));
    ASSERT_STR_EQ(s.dir, "Abc");
    ASSERT_EQ(s.canary[0], (char)0x5A);

    memset(&s, 0x5A, sizeof(s));
    ASSERT_FALSE(mod_entry_se_config_dir(
        "Mods/Abcd/ScriptExtender/Config.json", s.dir, sizeof(s.dir)));
    for (size_t i = 0; i < sizeof(s.canary); i++) {
        ASSERT_EQ(s.canary[i], (char)0x5A);
    }
}

/* Only the .pak extension is stripped, and only from the end. */
TEST(pak_stem_strips_one_extension_only) {
    char dir[64];
    ASSERT_TRUE(mod_se_dir_from_pak_name("/m/Mod.pak.pak", dir, sizeof(dir)));
    ASSERT_STR_EQ(dir, "Mod.pak");
    ASSERT_TRUE(mod_se_dir_from_pak_name("/m/pak.Mod", dir, sizeof(dir)));
    ASSERT_STR_EQ(dir, "pak.Mod");
    ASSERT_TRUE(mod_se_dir_from_pak_name("/m/A.PaK", dir, sizeof(dir)));
    ASSERT_STR_EQ(dir, "A");
}

/* mod_entry_se_config_dir uses strstr(), i.e. the FIRST occurrence of the
 * suffix, then requires it to terminate the entry. A path that repeats the
 * suffix must be rejected rather than yielding a bogus directory name. */
TEST(entry_dir_rejects_repeated_suffix) {
    char dir[64];
    ASSERT_FALSE(mod_entry_se_config_dir(
        "Mods/A/ScriptExtender/Config.json/ScriptExtender/Config.json",
        dir, sizeof(dir)));
}

TEST(entry_dir_rejects_case_variant_suffix) {
    char dir[64];
    ASSERT_FALSE(mod_entry_se_config_dir(
        "Mods/A/scriptextender/Config.json", dir, sizeof(dir)));
    ASSERT_FALSE(mod_entry_se_config_dir(
        "Mods/A/ScriptExtender/config.json", dir, sizeof(dir)));
}

TEST(entry_dir_rejects_bare_prefix) {
    char dir[64];
    ASSERT_FALSE(mod_entry_se_config_dir("Mods/", dir, sizeof(dir)));
    ASSERT_FALSE(mod_entry_se_config_dir("Mods", dir, sizeof(dir)));
    ASSERT_FALSE(mod_entry_se_config_dir("", dir, sizeof(dir)));
}

void register_mod_paths_tests(void) {
    printf("mod_paths:\n");
    RUN_TEST(pak_stem_basic);
    RUN_TEST(pak_stem_case_insensitive_ext);
    RUN_TEST(pak_stem_no_directory);
    RUN_TEST(pak_stem_no_extension);
    RUN_TEST(pak_stem_spaces_preserved);
    RUN_TEST(pak_stem_rejects_empty);
    RUN_TEST(pak_stem_rejects_overflow);
    RUN_TEST(entry_dir_match);
    RUN_TEST(entry_dir_match_with_spaces);
    RUN_TEST(entry_dir_rejects_wrong_prefix);
    RUN_TEST(entry_dir_rejects_nested_dir);
    RUN_TEST(entry_dir_rejects_deeper_path);
    RUN_TEST(entry_dir_rejects_empty_dir);
    RUN_TEST(entry_dir_rejects_overflow);
    RUN_TEST(pak_stem_boundary_exact_fit_and_one_over);
    RUN_TEST(entry_dir_boundary_exact_fit_and_one_over);
    RUN_TEST(pak_stem_strips_one_extension_only);
    RUN_TEST(entry_dir_rejects_repeated_suffix);
    RUN_TEST(entry_dir_rejects_case_variant_suffix);
    RUN_TEST(entry_dir_rejects_bare_prefix);
}
