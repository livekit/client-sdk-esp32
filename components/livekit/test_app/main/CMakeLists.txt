# NOTE: ../../core and ../../protocol are normally private include
# directories of the livekit component. The test app peeks at them so
# it can exercise the RPC manager modules directly without spinning up
# a full room. Do not copy this pattern into ordinary consumers.
idf_component_register(SRC_DIRS "."
                       INCLUDE_DIRS "../../include"
                                    "../../core"
                                    "../../protocol"
                       PRIV_REQUIRES test_utils unity nanopb)