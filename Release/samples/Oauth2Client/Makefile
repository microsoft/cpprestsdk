$(OUTPUT_DIR)/oauth2client: oauth2client
	cp oauth2client $@
	
oauth2client: ../../collateral/Samples/Oauth2Client/Oauth2Client.cpp ../../collateral/Samples/Oauth2Client/stdafx.cpp
	$(CXX) $(BASE_CXXFLAGS) -I$(CASABLANCA_INCLUDE_DIR) -include ../../collateral/Samples/Oauth2Client/stdafx.h $^ -o $@ -L$(OUTPUT_DIR) -lcasablanca -l$(BOOST_SYSTEM) -Wno-missing-include-dirs -Wno-unused-parameter -Wno-attributes -Wno-sign-compare -Wno-enum-compare
