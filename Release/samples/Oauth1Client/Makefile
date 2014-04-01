$(OUTPUT_DIR)/oauth1client: oauth1client
	cp oauth1client $@
	
oauth1client: ../../collateral/Samples/Oauth1Client/Oauth1Client.cpp ../../collateral/Samples/Oauth1Client/stdafx.cpp
	$(CXX) $(BASE_CXXFLAGS) -I$(CASABLANCA_INCLUDE_DIR) -include ../../collateral/Samples/Oauth1Client/stdafx.h $^ -o $@ -L$(OUTPUT_DIR) -lcasablanca -l$(BOOST_SYSTEM) -Wno-missing-include-dirs -Wno-unused-parameter -Wno-attributes -Wno-sign-compare -Wno-enum-compare
