# ##############################################################################
# vendor/SpaceMiT/boards/k1-rcpu/muse_pi_pro_rcpu/src/CMakeLists.txt
# ##############################################################################

set(SRCS k1r_boardinit.c k1r_appinit.c)

target_sources(board PRIVATE ${SRCS})
