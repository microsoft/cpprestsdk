$(OUTPUT_DIR)/blackjackclient: blackjackclient
	cp blackjackclient $@
	
blackjackclient: ../../../collateral/Samples/BlackJack/BlackJack_Client/BlackJackClient.cpp ../../../collateral/Samples/BlackJack/BlackJack_Client/stdafx.cpp
	$(CXX) $(BASE_CXXFLAGS) -I$(CASABLANCA_INCLUDE_DIR) -include ../../../collateral/Samples/BlackJack/BlackJack_Client/stdafx.h $^ -o $@ -L$(OUTPUT_DIR) -lcasablanca -l$(BOOST_SYSTEM) -Wno-missing-include-dirs -Wno-unused-parameter -Wno-attributes -Wno-sign-compare -Wno-enum-compare



