#include "mythics.hpp"

namespace eosio {

void token::create() {
     require_auth(get_self());

     auto sym = symbol("MYTHICS", 4); // MYTHICS is the token symbol with precision 4
     auto maximum_supply = asset(1000000000000, sym); /////////////////////////////////////////add decimal zeros

     stats statstable(get_self(), sym.code().raw());
     auto existing = statstable.find(sym.code().raw());
     check(existing == statstable.end(), "token with symbol already created");

     statstable.emplace(get_self(), [&](auto &s) {
        s.supply.symbol = sym;
        s.max_supply = maximum_supply;
        s.issuer = get_self();
     });
  }


 void token::issue(const asset &quantity, const string &memo) {
     require_auth(get_self());

     auto sym = quantity.symbol;
     auto mythicssym_code = symbol("MYTHICS", 4); // MYTHICS is the token symbol with precision 4
     check(sym.code() == mythicssym_code.code(), "This contract can handle MYTHICS tokens only.");
     check(sym.is_valid(), "invalid symbol name");
     check(memo.size() <= 256, "memo has more than 256 bytes");

     stats statstable(get_self(), sym.code().raw());
     auto existing = statstable.find(sym.code().raw());
     check(existing != statstable.end(), "token with symbol does not exist, create token before issue");

     const auto& existing_token = *existing;
     require_auth( existing_token.issuer );

     check(quantity.is_valid(), "invalid quantity");
     check(quantity.amount > 0, "must issue positive quantity");
     check(quantity.symbol == existing_token.supply.symbol, "symbol precision mismatch");
     check(quantity.amount <= existing_token.max_supply.amount - existing_token.supply.amount,
                                "quantity exceeds available supply");

     statstable.modify(existing_token, same_payer, [&](auto &s) {
        s.supply += quantity;
     });

     add_balance(existing_token.issuer, quantity, existing_token.issuer);
  }

  void token::retire(const name &owner, const asset &quantity, const string &memo) {
     auto sym = quantity.symbol;
     check(sym.is_valid(), "invalid symbol name");
     check(memo.size() <= 256, "memo has more than 256 bytes");
     auto mythicssym_code = symbol("MYTHICS", 4); // MYTHICS is the token symbol with precision 4
     check(sym.code() == mythicssym_code.code(), "This contract can handle MYTHICS tokens only.");

     stats statstable(get_self(), sym.code().raw());
     auto existing = statstable.find(sym.code().raw());
     check(existing != statstable.end(), "token with symbol does not exist");
     const auto &st = *existing;

     require_auth(owner);
     check(quantity.is_valid(), "invalid quantity");
     check(quantity.amount > 0, "must retire positive quantity");

     check(quantity.symbol == st.supply.symbol, "symbol precision mismatch");

     statstable.modify(st, same_payer, [&](auto &s) {
        s.supply -= quantity;
     });

     sub_balance(owner, quantity);
  }

  void token::transfer(const name &from,
                       const name &to,
                       const asset &quantity,
                       const string &memo) {
     check(from != to, "cannot transfer to self");
     require_auth(from);
     check(is_account(to), "to account does not exist");
     auto sym = quantity.symbol.code();

     auto mythicssym_code = symbol("MYTHICS", 4); // MYTHICS is the token symbol with precision 4
     check(sym.code() == mythicssym_code.code(), "This contract can handle MYTHICS tokens only.");
     stats statstable(get_self(), sym.raw());
     const auto &st = statstable.get(sym.raw());

     require_recipient(from);
     require_recipient(to);

     check(quantity.is_valid(), "invalid quantity");
     check(quantity.amount > 0, "must transfer positive quantity");
     check(quantity.symbol == st.supply.symbol, "symbol precision mismatch");
     check(memo.size() <= 256, "memo has more than 256 bytes");

     auto payer = has_auth(to) ? to : from;

    if(tax_enabled)
    {
        //If tax is enabled calcualte tax amount and subtract from transfer amount
        uint32_t taxamount = (quantity*tax_rate)/100;
        asset tax(taxamount, mythicssym_code);    // allow set amount of tokens to be recieved
        asset send_quantity((quantity-taxamount), mythicssym_code);    // allow set amount of tokens to be recieved
         sub_balance(from, quantity);
         add_balance(to, send_quantity, payer);
         retire(from, tax, "TAX");
    }
    else
    {
         sub_balance(from, quantity);
         add_balance(to, quantity, payer);
    }

  }

void token::sub_balance( const name& owner, const asset& value ) {
   accounts from_acnts( get_self(), owner.value );

   const auto& from = from_acnts.get( value.symbol.code().raw(), "no balance object found" );
   check( from.balance.amount >= value.amount, "overdrawn balance" );

   from_acnts.modify( from, owner, [&]( auto& a ) {
         a.balance -= value;
      });
}

void token::add_balance( const name& owner, const asset& value, const name& ram_payer )
{
   accounts to_acnts( get_self(), owner.value );
   auto to = to_acnts.find( value.symbol.code().raw() );
   if( to == to_acnts.end() ) {
      to_acnts.emplace( ram_payer, [&]( auto& a ){
        a.balance = value;
      });
   } else {
      to_acnts.modify( to, same_payer, [&]( auto& a ) {
        a.balance += value;
      });
   }
}

void token::enable_faucet(const bool state)
{
     require_auth(get_self());
     faucet_enabled = state;
}
void token::faucet_time(const uint32_t hours)
{
     require_auth(get_self());
     faucet_time = hours*60*60;
}
void token::faucet_amount(const uint32_t amount)
{
     require_auth(get_self());
     faucet_amount = amount*10000;
}

void token::enable_tax(const bool state)
{
     require_auth(get_self());
     tax_enabled = state;
}
void token::tax_rate(const uint32_t rate)
{
     require_auth(get_self());
     tax_rate = amount;
}

void token::faucet(const name &owner)
{
     check(_self != owner, "Cannot use faucet from MYTHICS owner account.");

     // Check if the user has used the faucet before
     faucets faucets_table(get_self(), sym.raw());
     auto it = faucets_table.find(owner.value);
     if(it == faucets_table.end()){
        // Register the user so it will be tracked for next use
        faucets_table.emplace(owner, [&](auto &row) {
            row.account = owner;
            row.last_recieved = now();
        });
     }
     else{
        // Check that the required time has passed since last time faucet was used.
        check(now() > it.last_recieved + faucet_time, "You need to wait longer before using the faucet again.");
     }
    
     require_auth(owner);
     require_recipient(_self); // from
     require_recipient(owner); // to

     auto sym = symbol("MYTHICS", 4); // MYTHICS is the token symbol with precision 4
     asset faucet_asset(faucet_amount, sym);    // allow set amount of tokens to be recieved

     sub_balance(_self, faucet_asset);
     add_balance(owner, faucet_asset, owner);

    //update the last_recieved to current time
    faucets_table.modify( it, same_payer, [&]( auto& a ) {
        a.last_recieved = now();
    });
  }


void token::open( const name& owner, const symbol& symbol, const name& ram_payer )
{
   require_auth( ram_payer );

   check( is_account( owner ), "owner account does not exist" );

   auto sym_code_raw = symbol.code().raw();
   stats statstable( get_self(), sym_code_raw );
   const auto& st = statstable.get( sym_code_raw, "symbol does not exist" );
   check( st.supply.symbol == symbol, "symbol precision mismatch" );

   accounts acnts( get_self(), owner.value );
   auto it = acnts.find( sym_code_raw );
   if( it == acnts.end() ) {
      acnts.emplace( ram_payer, [&]( auto& a ){
        a.balance = asset{0, symbol};
      });
   }
}

void token::close( const name& owner, const symbol& symbol )
{
   require_auth( owner );
   accounts acnts( get_self(), owner.value );
   auto it = acnts.find( symbol.code().raw() );
   check( it != acnts.end(), "Balance row already deleted or never existed. Action won't have any effect." );
   check( it->balance.amount == 0, "Cannot close because the balance is not zero." );
   acnts.erase( it );
}

} /// namespace eosio