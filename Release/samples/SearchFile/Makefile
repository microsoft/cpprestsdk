$(OUTPUT_DIR)/SearchFile: searchfile
	mv searchfile $@

searchfile: ../../collateral/Samples/SearchFile/searchfile.cpp 
	$(CXX) $(BASE_CXXFLAGS) -I$(CASABLANCA_INCLUDE_DIR) $^ -o $@ -L$(OUTPUT_DIR) -lcasablanca -lboost_thread -Wno-sign-compare -Wno-unused-parameter 
