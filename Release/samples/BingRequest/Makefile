$(OUTPUT_DIR)/BingRequest: ../../collateral/Samples/BingRequest/bingrequest.cpp 
	$(CXX) $(BASE_CXXFLAGS) -I$(CASABLANCA_INCLUDE_DIR) $^ -o $@ -L$(OUTPUT_DIR) -lcasablanca -l$(BOOST_SYSTEM) -l$(BOOST_THREAD) -Wno-sign-compare -Wno-unused-parameter 
