$(OUTPUT_DIR)/blackjackserver: blackjackserver
	cp blackjackserver $@

blackjackserver: ../../../collateral/Samples/BlackJack/BlackJack_Server/BlackJack_Server.cpp ../../../collateral/Samples/BlackJack/BlackJack_Server/Dealer.cpp ../../../collateral/Samples/BlackJack/BlackJack_Server/stdafx.cpp ../../../collateral/Samples/BlackJack/BlackJack_Server/Table.cpp
	$(CXX) $(BASE_CXXFLAGS) -I$(CASABLANCA_INCLUDE_DIR) -include ../../../collateral/Samples/BlackJack/BlackJack_Server/stdafx.h $^ -o $@ -L$(OUTPUT_DIR) -lcasablanca -l$(BOOST_SYSTEM) -Wno-missing-include-dirs -Wno-unused-parameter -Wno-attributes -Wno-sign-compare -Wno-enum-compare



