// Copyright 2011-2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "third_party/zynamics/bindiff/sqlite.h"

#include <cstring>
#include <stdexcept>

#include "third_party/absl/strings/str_cat.h"
#include "third_party/sqlite/src/sqlite3.h"

namespace security::bindiff {

SqliteDatabase::SqliteDatabase()
    : database_(nullptr) {
}

SqliteDatabase::SqliteDatabase(const char* filename)
    : database_(nullptr) {
  Connect(filename);
}

SqliteDatabase::~SqliteDatabase() {
  Disconnect();
}

void SqliteDatabase::Connect(const char* filename) {
  if (database_) {
    throw std::runtime_error("database already open");
  }

  if (sqlite3_open(filename, &database_) != SQLITE_OK) {
    const std::string error(sqlite3_errmsg(database_));
    sqlite3_close(database_);
    database_ = nullptr;
    throw std::runtime_error(absl::StrCat("failed opening database: '", error,
                                          "', filename: '", filename, "'"));
  }

  if (!database_) {
    throw std::runtime_error("failed opening database");
  }
}

void SqliteDatabase::Disconnect() {
  if (!database_) {
    return;
  }

  sqlite3_close(database_);
  database_ = nullptr;
}

std::shared_ptr<SqliteStatement> SqliteDatabase::Statement(
    const char* statement) {
  return std::make_shared<SqliteStatement>(this, statement);
}

void SqliteDatabase::Begin() {
  Statement("begin transaction")->Execute();
}

void SqliteDatabase::Commit() {
  Statement("commit transaction")->Execute();
}

void SqliteDatabase::Rollback() {
  Statement("rollback transaction")->Execute();
}

SqliteStatement::SqliteStatement(SqliteDatabase* database,
                                 const char* statement)
    : database_(database->database_),
      statement_(nullptr),
      parameter_(0),
      column_(0),
      got_data_(false) {
  if (sqlite3_prepare_v2(database_, statement, strlen(statement), &statement_,
                         nullptr) != SQLITE_OK) {
    const std::string error(sqlite3_errmsg(database_));
    throw std::runtime_error(absl::StrCat("error preparing statement '",
                                          statement, "', '", error, "'"));
  }
}

SqliteStatement::~SqliteStatement() {
  sqlite3_finalize(statement_);
}

SqliteStatement& SqliteStatement::BindInt(int value) {
  sqlite3_bind_int(statement_, ++parameter_, value);
  return *this;
}

SqliteStatement& SqliteStatement::BindInt64(int64_t value) {
  sqlite3_bind_int64(statement_, ++parameter_, value);
  return *this;
}

SqliteStatement& SqliteStatement::BindDouble(double value) {
  sqlite3_bind_double(statement_, ++parameter_, value);
  return *this;
}

// This creates a copy of the string which may well be undesired/inefficient.
SqliteStatement& SqliteStatement::BindText(absl::string_view value) {
  sqlite3_bind_text(statement_, ++parameter_, value.data(), value.size(),
                    SQLITE_TRANSIENT);
  return *this;
}

SqliteStatement& SqliteStatement::BindNull() {
  sqlite3_bind_null(statement_, ++parameter_);
  return *this;
}

SqliteStatement& SqliteStatement::Into(int* value, bool* is_null) {
  if (is_null) {
    *is_null = sqlite3_column_type(statement_, column_) == SQLITE_NULL;
  }
  *value = sqlite3_column_int(statement_, column_);
  ++column_;
  return *this;
}

SqliteStatement& SqliteStatement::Into(int64_t * value, bool* is_null) {
  if (is_null) {
    *is_null = sqlite3_column_type(statement_, column_) == SQLITE_NULL;
  }
  *value = sqlite3_column_int64(statement_, column_);
  ++column_;
  return *this;
}

SqliteStatement& SqliteStatement::Into(Address* value, bool* is_null) {
  if (is_null) {
    *is_null = sqlite3_column_type(statement_, column_) == SQLITE_NULL;
  }
  *value = static_cast<Address>(
      sqlite3_column_int64(statement_, column_));
  ++column_;
  return *this;
}

SqliteStatement& SqliteStatement::Into(double* value, bool* is_null) {
  if (is_null) {
    *is_null = sqlite3_column_type(statement_, column_) == SQLITE_NULL;
  }
  *value = sqlite3_column_double(statement_, column_);
  ++column_;
  return *this;
}

SqliteStatement& SqliteStatement::Into(std::string* value, bool* is_null) {
  if (is_null) {
    *is_null = sqlite3_column_type(statement_, column_) == SQLITE_NULL;
  }
  if (auto* data = reinterpret_cast<const char*>(
          sqlite3_column_text(statement_, column_)))
    value->assign(data);
  ++column_;
  return *this;
}

SqliteStatement& SqliteStatement::Execute() {
  const int return_code = sqlite3_step(statement_);
  if (return_code != SQLITE_ROW && return_code != SQLITE_DONE) {
    const std::string error(sqlite3_errmsg(database_));
    throw std::runtime_error(absl::StrCat("error executing statement '",
                                          sqlite3_sql(statement_), "', '",
                                          error, "'"));
  }

  parameter_ = 0;
  column_ = 0;
  got_data_ = return_code == SQLITE_ROW;
  return *this;
}

SqliteStatement& SqliteStatement::Reset() {
  sqlite3_reset(statement_);
  got_data_ = false;
  return *this;
}

bool SqliteStatement::GotData() const {
  return got_data_;
}

}  // namespace security::bindiff
