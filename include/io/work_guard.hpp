
//  Pierre - Custom Light Show for Wiss Landing
//  Copyright (C) 2022  Tim Hughey
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  https://www.wisslanding.com

#pragma once

#define BOOST_ASIO_DISABLE_BOOST_COROUTINE

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/system/errc.hpp>
#include <boost/system/error_code.hpp>

namespace pierre {

namespace asio = boost::asio;

using io_context = asio::io_context;
using work_guard = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;
using work_guard_tp = boost::asio::executor_work_guard<boost::asio::thread_pool::executor_type>;

} // namespace pierre