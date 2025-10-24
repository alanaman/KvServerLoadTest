#pragma once

#include <sqlpp11/table.h>
#include <sqlpp11/data_types.h>
#include <sqlpp11/char_sequence.h>

namespace tables
{
  // Define the columns for our table
  namespace KeyValue_
  {
    struct Key
    {
      struct _alias_t
      {
        static constexpr const char _literal[] =  "key";
        using _name_t = sqlpp::make_char_sequence<sizeof(_literal), _literal>;
        template<typename T>
        struct _member_t
        {
          T key;
          T& operator()() { return key; }
          const T& operator()() const { return key; }
        };
      };
      // 'id' is an integer, auto-incrementing key
      using _traits = sqlpp::make_traits<sqlpp::integer>;
    };

    struct Value
    {
      struct _alias_t
      {
        static constexpr const char _literal[] =  "value";
        using _name_t = sqlpp::make_char_sequence<sizeof(_literal), _literal>;
        template<typename T>
        struct _member_t
        {
          T value;
          T& operator()() { return value; }
          const T& operator()() const { return value; }
        };
      };
      // 'name' is a text field that can be null
      using _traits = sqlpp::make_traits<sqlpp::text, sqlpp::tag::can_be_null>;
    };
  }

  // Define the table itself, named 'tab_sample'
  struct KeyValueTable : sqlpp::table_t<KeyValueTable,
                           KeyValue_::Key,
                           KeyValue_::Value>
  {
    using _value_type = sqlpp::no_value_t;
    struct _alias_t
    {
      static constexpr const char _literal[] =  "key_value";
      using _name_t = sqlpp::make_char_sequence<sizeof(_literal), _literal>;
      template<typename T>
      struct _member_t
      {
        T keyValue;
        T& operator()() { return keyValue; }
        const T& operator()() const { return keyValue; }
      };
    };
  };
}