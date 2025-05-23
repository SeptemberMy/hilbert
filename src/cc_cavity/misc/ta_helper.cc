/*
 *  @BEGIN LICENSE
 *
 *  Hilbert: a space for quantum chemistry plugins to Psi4
 *
 *  Copyright (c) 2020 by its authors (LICENSE).
 *
 *  The copyrights for code used from other parties are included in
 *  the corresponding files.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program.  If not, see http://www.gnu.org/licenses/.
 *
 *  @END LICENSE
 */

#include "ta_helper.h"

using namespace std;
using namespace TA;
namespace TA_Helper {

    TiledRange makeRange(const initializer_list<size_t> &N) { // create range
        vector<size_t> Nblk;
        vector<TiledRange1> trange;

        if (tile_size_ == 0)
            throw runtime_error("tile_size_ is 0. Please set tile_size_ to a value > 0. (TA_Helper::makeRange())");

        // create tiled range 1 for each dimension
        for (auto n: N) {
            Nblk.push_back(0);

            // create tiles
            for (size_t i = tile_size_; i < n; i += tile_size_) {
                Nblk.push_back(i);
            }
            Nblk.push_back(n);

            TiledRange1 range(Nblk.begin(), Nblk.end());
            trange.push_back(range);
            Nblk.clear();
        }

        //Create tensor
        TiledRange TR(trange);
        return TR;
    }
    TiledRange makeRange(const vector<size_t> &N) { // create range
        vector<size_t> Nblk;
        vector<TiledRange1> trange;

        if (tile_size_ == 0)
            throw runtime_error("tile_size_ is 0. Please set tile_size_ to a value > 0. (TA_Helper::makeRange())");

        // create tiled range 1 for each dimension
        for (auto n: N) {
            Nblk.push_back(0);

            // create tiles
            for (size_t i = tile_size_; i < n; i += tile_size_) {
                Nblk.push_back(i);
            }
            Nblk.push_back(n);

            TiledRange1 range(Nblk.begin(), Nblk.end());
            trange.push_back(range);
            Nblk.clear();
        }

        //Create tensor
        TiledRange TR(trange);
        return TR;
    }

    TArrayD makeTensor(World &world, const initializer_list<size_t> &N, bool fillZero) {
        TArrayD array(world, makeRange(N));

        if (fillZero)
            array.fill(0.0);

        world.gop.fence();
        return array;
    }

    TArrayD makeTensor(World &world, const initializer_list<size_t> &N, const double *data,
               initializer_list<size_t> Off) {
        // create tensor
        TArrayD array = makeTensor(world, N, false);

        // create array of dimension sizes for indexing
        size_t N_size = N.size();
        auto *dim_sizes = (size_t *) alloca(N_size * sizeof(size_t));

        dim_sizes[N_size - 1] = 1; // last index dimension size is 1

        // {o,v,o,v} -> i*v*o*v + a*o*v + j*v + b
        auto N_it = N.begin(); // create iterator for N
        for (int i = ((int) N_size) - 2; i >= 0; --i) {
            // calculate dimension size for index i
            dim_sizes[i] = dim_sizes[i + 1] * N_it[i + 1];
        }

        if (Off.size() == 0) { // if Off is empty

            // fill tensor with data
            array.init_elements([data, N_size, dim_sizes](const typename TArrayD::index &I) {
                                             size_t index = 0;
                                             for (size_t i = 0; i < N_size; i++) {
                                                 index += I[i] * dim_sizes[i];
                                             }
                                             // get value
                                             double val = data[index];
                                             return val; // return value
                                         }
            );
            world.gop.fence();
            return array; // return tensor

        } else if (Off.size() == N.size()) { // if Off is not empty
            auto off_it = Off.begin(); // create iterator for Off

            // fill tensor with data
            array.init_elements(
                    [data, N_size, off_it, dim_sizes](
                            const typename TArrayD::index &I) { // fill tensor with data
                        size_t index = 0;
                        // calculate index
                        for (size_t i = 0; i < N_size; i++) {
                            index += (I[i] + off_it[i]) * dim_sizes[i];
                        }
                        // get value
                        double val = data[index];
                        return val; // return value
                    }
            );
            world.gop.fence();
            return array;
        } else {
            throw runtime_error("TA_Helper::makeTensor: Off.size() != N.size() and Off != {}");
        }
    }

    TArrayD makeTensor(World &world,
               const initializer_list<size_t> &NL,
               const initializer_list<size_t> &NR,
               const double *const *data,
               initializer_list<size_t> Off) {

        vector<size_t> N(NL.begin(), NL.end());
        N.insert(N.end(), NR.begin(), NR.end());

        // create tensor
        TArrayD array(world, makeRange(N));

        // create array of dimension sizes for indexing
        size_t NL_size = NL.size();
        size_t NR_size = NR.size();
        auto *dim_sizes_L = (size_t *) alloca(NL_size * sizeof(size_t));
        auto *dim_sizes_R = (size_t *) alloca(NR_size * sizeof(size_t));

        if (NL_size == 0 || NR_size == 0) {
            throw runtime_error("TA_Helper::makeTensor: NL.size() == 0 || NR.size() == 0");
        }

        dim_sizes_L[NL_size - 1] = 1; // last index dimension size is 1
        dim_sizes_R[NR_size - 1] = 1; // last index dimension size is 1

        auto NL_it = NL.begin(); // create iterator for N
        for (int i = ((int) NL_size) - 2; i >= 0; --i) {
            // calculate dimension size for index i
            dim_sizes_L[i] = dim_sizes_L[i + 1] * NL_it[i + 1];
        }

        auto NR_it = NR.begin(); // create iterator for N
        for (int i = ((int) NR_size) - 2; i >= 0; --i) {
            // calculate dimension size for index i
            dim_sizes_R[i] = dim_sizes_R[i + 1] * NR_it[i + 1];
        }

        if (Off.size() == 0) { // if Off is empty

            // fill tensor with data
            array.init_elements(
                    [data, NL_size, NR_size, dim_sizes_L, dim_sizes_R](const typename TArrayD::index &I) {
                        size_t indexL = 0, indexR = 0;
                        for (size_t i = 0; i < NL_size; i++)
                            indexL += I[i] * dim_sizes_L[i];
                        for (size_t i = 0; i < NR_size; i++)
                            indexR += I[i + NL_size] * dim_sizes_R[i];

                        // get value
                        double val = data[indexL][indexR];
                        return val; // return value
                    }
            );
            world.gop.fence();
            return array; // return tensor

        } else if (Off.size() == N.size()) { // if Off is not empty
            auto off_it = Off.begin(); // create iterator for Off

            // fill tensor with data
            array.template init_elements([data, NL_size, NR_size, dim_sizes_L, dim_sizes_R, off_it](
                                                 const typename TArrayD::index &I) { // fill tensor with data
                                             size_t indexL = 0, indexR = 0;
                                             for (size_t i = 0; i < NL_size; i++)
                                                 indexL += (I[i] + off_it[i]) * dim_sizes_L[i];
                                             for (size_t i = 0; i < NR_size; i++)
                                                 indexR += (I[i + NL_size] + off_it[i + NL_size]) * dim_sizes_R[i];

                                             // get value
                                             double val = data[indexL][indexR];
                                             return val; // return value
                                         }
            );
            world.gop.fence();
            return array;
        } else {
            throw runtime_error("TA_Helper::makeTensor: Off.size() != N.size() and Off != {}");
        }
    }
}