//--------------------------------------------------------------------------
// Copyright (C) 2018-2024 Cisco and/or its affiliates. All rights reserved.
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License Version 2 as published
// by the Free Software Foundation.  You may not use, modify or distribute
// this program under any other version of the GNU General Public License.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//--------------------------------------------------------------------------

// ips_s7comm_data.cc author Yarin Peretz <yarinp123@gmail.com>
// based on work by Jeffrey Gu <jgu@cisco.com>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <iostream> // For debug output
#include <numeric>  // For std::accumulate
#include "framework/ips_option.h"
#include "framework/module.h"
#include "hash/hash_key_operations.h"
#include "protocols/packet.h"
#include "profiler/profiler.h"

#include "s7comm.h"

using namespace snort;

static const char* s_name = "s7comm_di_data";

//-------------------------------------------------------------------------
// data option
//-------------------------------------------------------------------------

static THREAD_LOCAL ProfileStats s7comm_data_prof;

class S7commDataOption : public IpsOption
{
public:
    S7commDataOption(const std::vector<uint8_t>& v) : IpsOption(s_name), data(v) {}

    uint32_t hash() const override;
    bool operator==(const IpsOption&) const override;
    EvalStatus eval(Cursor&, Packet*) override;

private:
    std::vector<uint8_t> data;
};

uint32_t S7commDataOption::hash() const
{
    uint32_t a = std::accumulate(data.begin(), data.end(), 0u), b = IpsOption::hash(), c = 0;
    mix(a, b, c);
    finalize(a, b, c);
    return c;
}

bool S7commDataOption::operator==(const IpsOption& ips) const
{
    if (!IpsOption::operator==(ips))
        return false;

    const S7commDataOption& rhs = (const S7commDataOption&)ips;
    return (data == rhs.data);
}

IpsOption::EvalStatus S7commDataOption::eval(Cursor&, Packet* p)
{
    RuleProfile profile(s7comm_data_prof);

    if (!p->flow)
        return NO_MATCH;

    if (!p->is_full_pdu())
        return NO_MATCH;

    S7commFlowData* mfd = (S7commFlowData*)p->flow->get_flow_data(S7commFlowData::inspector_id);

    if (!mfd)
        return NO_MATCH;

    for (const auto& dataItem : mfd->ssn_data.data_items)
    {        
        if (dataItem.data == data)
            return MATCH;
    }

    return NO_MATCH;
}

//-------------------------------------------------------------------------
// module
//-------------------------------------------------------------------------

static const Parameter s_params[] =
{
    { "~", Parameter::PT_STRING, nullptr, nullptr, "data to match" },
    { nullptr, Parameter::PT_MAX, nullptr, nullptr, nullptr }
};

#define s_help \
    "rule option to check s7comm data"

class S7commDataModule : public Module
{
public:
    S7commDataModule() : Module(s_name, s_help, s_params) {}

    bool set(const char*, Value&, SnortConfig*) override;
    ProfileStats* get_profile() const override { return &s7comm_data_prof; }
    Usage get_usage() const override { return DETECT; }

public:
    std::vector<uint8_t> data;
};

bool S7commDataModule::set(const char*, Value& v, SnortConfig*)
{
    assert(v.is("~"));
    long n;

    if (v.strtol(n)) {
        std::string data_str = v.get_string();
        
        if (data_str.rfind("0x", 0) == 0) {
            data_str.erase(0, 2);  // Remove the first two characters ("0x")
        }
        // Convert the hex string into a byte vector
        for (size_t i = 0; i < data_str.length(); i += 2) {
            std::string byteString = data_str.substr(i, 2); // Get two characters
            uint8_t byte = static_cast<uint8_t>(strtol(byteString.c_str(), nullptr, 16));
            data.push_back(byte);
        }

        // Now 'data' contains the bytes represented by the hex string
    }

    return true;
}

static Module* mod_ctor()
{
    return new S7commDataModule;
}

static void mod_dtor(Module* m)
{
    delete m;
}

static IpsOption* opt_ctor(Module* m, IpsInfo&)
{
    S7commDataModule* mod = (S7commDataModule*)m;
    return new S7commDataOption(mod->data);
}

static void opt_dtor(IpsOption* p)
{
    delete p;
}

static const IpsApi ips_api =
{
    {
        PT_IPS_OPTION,
        sizeof(IpsApi),
        IPSAPI_VERSION,
        0,
        API_RESERVED,
        API_OPTIONS,
        s_name,
        s_help,
        mod_ctor,
        mod_dtor
    },
    OPT_TYPE_DETECTION,
    0, PROTO_BIT__TCP,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    opt_ctor,
    opt_dtor,
    nullptr
};

const BaseApi* ips_s7comm_di_data = &ips_api.base;
