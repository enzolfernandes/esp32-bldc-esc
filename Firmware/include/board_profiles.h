/*
 * board_profiles.h — Perfis compiláveis PS4 (bancada vs DS4 original).
 *
 * Incluído por board_config.h antes das flags PS4_*.
 */

#ifndef BOARD_PROFILES_H
#define BOARD_PROFILES_H

#define PS4_PROFILE_BENCH_STABLE   0
#define PS4_PROFILE_DS4_ORIGINAL   1

#ifndef PS4_ACTIVE_PROFILE
#define PS4_ACTIVE_PROFILE PS4_PROFILE_DS4_ORIGINAL
#endif

#endif /* BOARD_PROFILES_H */
