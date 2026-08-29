# parser_hashes.cmake
# SHA-256 hashes of evaluator.y and evaluator.l corresponding to the
# pre-generated evaluator_tab.cpp and so_eval.ic committed in the source tree.
#
# When these hashes differ from the current .y/.l file contents a non-fatal
# warning is emitted at build time to signal that the grammar has changed and
# the committed generated files need to be updated.
#
# Update by running:
#   cmake --build <build_dir> --target update_parser_sources

set(EVALUATOR_Y_HASH "935057e1d5338839cf71e35489cca62d509ddf98604af780624aef744e8117ae")
set(EVALUATOR_L_HASH "10b0ddf713a6cec9ae299439d6439c3d37b6dcd62c093e68c8f38edbfb033f1e")
