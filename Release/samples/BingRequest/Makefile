$(OUTPUT_DIR)/BingRequest: bingrequest
	cp bingrequest $@

bingrequest: ../../collateral/Samples/BingRequest/bingrequest.cpp 
	$(CXX) $(BASE_CXXFLAGS) -I$(CASABLANCA_INCLUDE_DIR) $^ -o $@ -L$(OUTPUT_DIR) -lcasablanca -lboost_thread -Wno-sign-compare -Wno-unused-parameter 
