/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */

#include "rankblock.token.hpp"

const symbol_type rankblock_SYMBOL = eosio::symbol_type(eosio::string_to_symbol(4, "RB"));

void rankblocktoken::create(account_name issuer,
													 asset maximum_supply)
{
	require_auth(_self);

	auto sym = maximum_supply.symbol;
	eosio_assert(sym.is_valid(), "e.1"); //invalid symbol name
	eosio_assert(maximum_supply.is_valid(), "e.2"); // invalid supply
	eosio_assert(maximum_supply.amount > 0, "e.3"); // max-supply must be positive

	stats statstable(_self, sym.name());
	auto existing = statstable.find(sym.name());
	eosio_assert(existing == statstable.end(), "err.4"); // token with symbol already exists

	statstable.emplace(_self, [&](auto &s) {
		s.supply.symbol = maximum_supply.symbol;
		s.max_supply = maximum_supply;
		s.issuer = issuer;
	});
}

void rankblocktoken::issue(account_name to, asset quantity, string memo)
{
	auto sym = quantity.symbol;
	eosio_assert(sym.is_valid(), "e.1"); // invalid symbol name
	eosio_assert(memo.size() <= 256, "e.4"); // memo has more than 256 bytes

	auto sym_name = sym.name();
	stats statstable(_self, sym_name);
	auto existing = statstable.find(sym_name);
	eosio_assert(existing != statstable.end(), "e.5"); // token with symbol does not exist, create token before issue
	const auto &st = *existing;

	require_auth(st.issuer);
	eosio_assert(quantity.is_valid(), "e.6"); // invalid quantity
	eosio_assert(quantity.amount > 0, "e.7"); // must issue positive quantity

	eosio_assert(quantity.symbol == st.supply.symbol, "e.8"); // symbol precision mismatch
	eosio_assert(quantity.amount <= st.max_supply.amount - st.supply.amount, "e.9"); // quantity exceeds available supply

	statstable.modify(st, 0, [&](auto &s) {
		s.supply += quantity;
	});

	add_balance(st.issuer, quantity, st.issuer);

	if (to != st.issuer)
	{
		SEND_INLINE_ACTION(*this, transfer, {st.issuer, N(active)}, {st.issuer, to, quantity, memo});
	}
}

void rankblocktoken::transfer(account_name from,
														 account_name to,
														 asset quantity,
														 string memo)
{
	eosio_assert(from != to, "e.10"); // cannot transfer to self
	require_auth(from);
	eosio_assert(is_account(to), "e.11"); // to account does not exist
	auto sym = quantity.symbol.name();
	stats statstable(_self, sym);
	const auto &st = statstable.get(sym);

	require_recipient(from);
	require_recipient(to);

	eosio_assert(quantity.is_valid(), "e.6"); // invalid quantity
	eosio_assert(quantity.amount > 0, "e.12"); // must transfer positive quantity
	eosio_assert(quantity.symbol == st.supply.symbol, "e.13"); // symbol precision mismatch
	eosio_assert(memo.size() <= 256, "e.4"); // memo has more than 256 bytes

	sub_balance(from, quantity);
	add_balance(to, quantity, from);
}

void rankblocktoken::sub_balance(account_name owner, asset value)
{
	accounts from_acnts(_self, owner);

	const auto &from = from_acnts.get(value.symbol.name(), "e.14"); // no balance object found
	eosio_assert(from.balance.amount >= value.amount, "e.15"); // overdrawn balance

	if (from.balance.amount == value.amount)
	{
		from_acnts.erase(from);
	}
	else
	{
		from_acnts.modify(from, owner, [&](auto &a) {
			a.balance -= value;
		});
	}
}

void rankblocktoken::add_balance(account_name owner, asset value, account_name ram_payer)
{
	accounts to_acnts(_self, owner);
	auto to = to_acnts.find(value.symbol.name());
	if (to == to_acnts.end())
	{
		to_acnts.emplace(ram_payer, [&](auto &a) {
			a.balance = value;
		});
	}
	else
	{
		to_acnts.modify(to, 0, [&](auto &a) {
			a.balance += value;
		});
	}
}

void rankblocktoken::regairdrop(account_name receiver)
{
  accounts to_acnts(_self, receiver);
  auto to = to_acnts.find(rankblock_SYMBOL.name());
  eosio_assert(to == to_acnts.end(), "e.16"); // This receiver is already resgisterd.
  if (to == to_acnts.end())
  {
    to_acnts.emplace(receiver, [&](auto &a) {
    });
  }
}

EOSIO_ABI(rankblocktoken, (create)(issue)(transfer)(regairdrop))