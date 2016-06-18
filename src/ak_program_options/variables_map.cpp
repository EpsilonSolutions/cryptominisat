/*
 * ak_program_options
 *
 * Copyright (c) 2015, Axel Kemper. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation
 * version 2.0 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301  USA
 *
 * Package ak_program_options was derived from boost::program_options
 * as a simplified and restricted package suitable for cryptominisat.
 * Original copyright:
 * Copyright Vladimir Prus 2002-2004.
 * Distributed under the Boost Software License, Version 1.0.
 * (See http://www.boost.org/LICENSE_1_0.txt)
 */

#include <assert.h>
#include <map>
#include <iostream>

#include "command_line_parser.h"
#include "akpo_getopt.h"
#include "positional_options.h"
#include "variables_map.h"
#include <string.h>

//  global variables for getopt_long()
int   ac_opterr;   /* if error message should be printed */
int   ak_optind;   /* index into parent argv vector */
int   ak_optopt;   /* character checked for validity */
int   ak_optreset; /* reset getopt */
char* ak_optarg;   /* argument associated with option */

namespace ak_program_options {

/** Stores in 'vm' all options that are defined in 'options'.
If 'vm' already has a non-defaulted value of an option, that value
is not changed, even if 'options' specify some value.
*/
void store(const basic_parsed_options* options, variables_map& vm)
{
    std::string short_options(options->short_options().c_str());

    option* long_options = options->long_options();
    const positional_options_description* positional_options = options->get_positional_description();
    int index;
    unsigned pos_option_index = 0;

    //  remember options for deconstruction
    vm.remember_options(options);

    //  start with argv[1] as first parameter token
    ak_optind = 1;

    while (true) {
        index = 0;
        const int opt = getopt_long(
                            options->argc, options->argv, short_options.c_str(), long_options, &index);

        if ((char)opt == ':') {
            throw_exception<invalid_option_value>(invalid_option_value(options->argv[ak_optind-2], ""));
        }

        if (-1 == opt) {
            //  no named parameter found
            //  might be a positional parameter
            if (ak_optind >= (int)options->argc) {
                //  end of command line reached
                break;
            }
            if (pos_option_index < positional_options->max_total_count()) {
                //  create an entry for positional parameter in variables map
                const std::string name = positional_options->name_for_position(pos_option_index++);
                const option_description* desc = options->findByName(name);
                //  use defined value iff avalaible or create a new string value if not
                value_semantic* sem = (desc == nullptr) ? value<std::string>() : desc->semantic();
                sem->set_value(options->argv[ak_optind++]);

                //  entry must not be present yet
                assert(vm.find(name) == vm.end());
                vm.insert(std::make_pair(name, sem));

                continue;
            } else {
                too_many_positional_options_error e;
                throw_exception<too_many_positional_options_error>(e);
            }

            break;
        }

        const option_description* desc = options->findById(opt);
        if (desc == nullptr) {
            ak_optind--;
            std::string name(options->argv[ak_optind]);
            unknown_option e(name);
            throw_exception<unknown_option>(e);
        }

        std::string name(desc->name());
        value_semantic* sem = desc->semantic();
        bool bInsertNeeded = (vm.find(name) == vm.end());

        if (!(bInsertNeeded || sem->composing())) {
            std::string name(desc->format_name());
            multiple_occurrences e(name);
            throw_exception<multiple_occurrences>(e);
        } else {
            if (sem != nullptr) {
                if (ak_optarg) {
                    sem->set_value(ak_optarg);
                } else if (sem->implicited()) {
                    sem->apply_implicit();
                }
            }
            if (bInsertNeeded) {
                vm.insert(std::make_pair(name, sem));
            }
        }
    }

    delete [] long_options;

    //  add options which have defaults and are not contained yet
    for (option_description* opt : options->descriptions().options()) {
        std::string name(opt->name());
        value_semantic* sem = opt->semantic();

        if (sem != nullptr) {
            bool bInsertNeeded = (vm.find(name) == vm.end()) && sem->defaulted();

            if (bInsertNeeded) {
                vm.insert(std::make_pair(name, sem));
            }
        }
    }
    delete options;
}

void notify(variables_map& vm)
{
    vm.notify();
}

variables_map::~variables_map()
{
    assert(m_options != nullptr);
    delete m_options;
}

const value_semantic
& variables_map::operator[](const std::string& name) const
{
    const value_semantic* v = get(name);

    return *v;
}

void variables_map::clear()
{
    std::map<std::string, value_semantic*>::clear();
}

const value_semantic
* variables_map::get(const std::string& name) const
{
    static value_semantic empty;
    const_iterator i = find(name);

    return (i == end()) ? &empty
           : i->second;
}

void variables_map::notify()
{
    // Not implemented: checks if all required options occur

    // Lastly, run notify actions.
    for (auto& kv : *this) {
        if (kv.second != nullptr) {
            kv.second->notify();
        }
    }
}

void variables_map::show_options()
{
    std::cout << "Entries in variables_map: " << this->size() << std::endl;

    for (auto& kv : *this) {
        std::cout << kv.first << ": ";

        if (kv.second != nullptr) {
            std::cout << kv.second->to_string();
        } else {
            std::cout << " <null>";
        }
        std::cout << std::endl;
    }

    std::cout << std::endl;
}

}