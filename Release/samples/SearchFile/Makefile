$(OUTPUT_DIR)/SearchFile: ../../collateral/Samples/SearchFile/searchfile.cpp
	$(CXX) $(BASE_CXXFLAGS) -I$(CASABLANCA_INCLUDE_DIR) $^ -o $@ -L$(OUTPUT_DIR) -lcasablanca -l$(BOOST_SYSTEM) -l$(BOOST_THREAD) -Wno-sign-compare -Wno-unused-parameter 
