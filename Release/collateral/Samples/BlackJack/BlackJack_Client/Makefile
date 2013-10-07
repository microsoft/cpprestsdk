$(OUTPUT_DIR)/blackjackclient: blackjackclient
	cp blackjackclient $@
	
blackjackclient: BlackJackClient.cpp stdafx.cpp
	$(CXX) $(BASE_CXXFLAGS) -I$(CASABLANCA_INCLUDE_DIR) -include stdafx.h $^ -o $@ -L$(OUTPUT_DIR) -lcasablanca -I$(shell pkg-config libxml++-2.6 --cflags) -lboost_system -Wno-missing-include-dirs -Wno-unused-parameter -Wno-attributes -Wno-sign-compare -Wno-enum-compare



