/*
  Copyright 2016 Statoil ASA.

  This file is part of the Open Porous Media project (OPM).

  OPM is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  OPM is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with OPM.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <opm/parser/eclipse/Parser/ParseContext.hpp>
#include <opm/parser/eclipse/Deck/Deck.hpp>
#include <opm/parser/eclipse/Deck/DeckItem.hpp>
#include <opm/parser/eclipse/Deck/DeckKeyword.hpp>
#include <opm/parser/eclipse/Deck/DeckRecord.hpp>
#include <opm/parser/eclipse/Deck/Section.hpp>
#include <opm/parser/eclipse/EclipseState/Tables/TableManager.hpp>
#include <opm/parser/eclipse/EclipseState/EclipseState.hpp>
#include <opm/parser/eclipse/EclipseState/Grid/EclipseGrid.hpp>
#include <opm/parser/eclipse/EclipseState/Grid/GridDims.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Connection.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/WellConnections.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Group.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Schedule.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/TimeMap.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Well.hpp>
#include <opm/parser/eclipse/EclipseState/SummaryConfig/SummaryConfig.hpp>

#include <ert/ecl/ecl_smspec.h>

#include <iostream>
#include <algorithm>
#include <array>

namespace Opm {

namespace {
    /*
      Small dummy decks which contain a list of keywords; observe that
      these dummy decks will be used as proper decks and MUST START
      WITH SUMMARY.
    */

    const Deck ALL_keywords = {
        "SUMMARY",
        "FAQR",  "FAQRG", "FAQT", "FAQTG", "FGIP", "FGIPG", "FGIPL",
        "FGIR",  "FGIT",  "FGOR", "FGPR",  "FGPT", "FOIP",  "FOIPG",
        "FOIPL", "FOIR",  "FOIT", "FOPR",  "FOPT", "FPR",   "FVIR",
        "FVIT",  "FVPR",  "FVPT", "FWCT",  "FWGR", "FWIP",  "FWIR",
        "FWIT",  "FWPR",  "FWPT",
        "GGIR",  "GGIT",  "GGOR", "GGPR",  "GGPT", "GOIR",  "GOIT",
        "GOPR",  "GOPT",  "GVIR", "GVIT",  "GVPR", "GVPT",  "GWCT",
        "GWGR",  "GWIR",  "GWIT", "GWPR",  "GWPT",
        "WBHP",  "WGIR",  "WGIT", "WGOR",  "WGPR", "WGPT",  "WOIR",
        "WOIT",  "WOPR",  "WOPT", "WPI",   "WTHP", "WVIR",  "WVIT",
        "WVPR",  "WVPT",  "WWCT", "WWGR",  "WWIR", "WWIT",  "WWPR",
        "WWPT",
        // ALL will not expand to these keywords yet
        "AAQR",  "AAQRG", "AAQT", "AAQTG"
    };

    const Deck GMWSET_keywords = {
        "SUMMARY",
        "GMCTG", "GMWPT", "GMWPR", "GMWPA", "GMWPU", "GMWPG", "GMWPO", "GMWPS",
        "GMWPV", "GMWPP", "GMWPL", "GMWIT", "GMWIN", "GMWIA", "GMWIU", "GMWIG",
        "GMWIS", "GMWIV", "GMWIP", "GMWDR", "GMWDT", "GMWWO", "GMWWT"
    };

    const Deck FMWSET_keywords = {
        "SUMMARY",
        "FMCTF", "FMWPT", "FMWPR", "FMWPA", "FMWPU", "FMWPF", "FMWPO", "FMWPS",
        "FMWPV", "FMWPP", "FMWPL", "FMWIT", "FMWIN", "FMWIA", "FMWIU", "FMWIF",
        "FMWIS", "FMWIV", "FMWIP", "FMWDR", "FMWDT", "FMWWO", "FMWWT"
    };


    const Deck PERFORMA_keywords = {
        "SUMMARY",
        "TCPU", "ELAPSED","NEWTON","NLINERS","NLINSMIN", "NLINSMAX","MLINEARS",
        "MSUMLINS","MSUMNEWT","TIMESTEP","TCPUTS","TCPUDAY","STEPTYPE","TELAPLIN"
    };


    /*
      The variable type 'ECL_SMSPEC_MISC_TYPE' is a catch-all variable
      type, and will by default internalize keywords like 'ALL' and
      'PERFORMA', where only the keywords in the expanded list should
      be included.
    */
    const std::set<std::string> meta_keywords = {"PERFORMA" , "ALL" , "FMWSET", "GMWSET"};

    /*
      This is a hardcoded mapping between 3D field keywords,
      e.g. 'PRESSURE' and 'SWAT' and summary keywords like 'RPR' and
      'BPR'. The purpose of this mapping is to maintain an overview of
      which 3D field keywords are needed by the Summary calculation
      machinery, based on which summary keywords are requested. The
      Summary calculations are implemented in the opm-output
      repository.
    */
    const std::map<std::string , std::set<std::string>> required_fields =  {
         {"PRESSURE", {"FPR" , "RPR" , "BPR"}},
         {"OIP"  , {"ROIP" , "FOIP" , "FOE"}},
         {"OIPL" , {"ROIPL" ,"FOIPL" }},
         {"OIPG" , {"ROIPG" ,"FOIPG"}},
         {"GIP"  , {"RGIP" , "FGIP"}},
         {"GIPL" , {"RGIPL" , "FGIPL"}},
         {"GIPG" , {"RGIPG", "FGIPG"}},
         {"WIP"  , {"RWIP" , "FWIP"}},
         {"SWAT" , {"BSWAT"}},
         {"SGAS" , {"BSGAS"}}
    };



void handleMissingWell( const ParseContext& parseContext , const std::string& keyword, const std::string& well) {
    std::string msg = std::string("Error in keyword:") + keyword + std::string(" No such well: ") + well;
    if (parseContext.get( ParseContext::SUMMARY_UNKNOWN_WELL) == InputError::WARN)
        std::cerr << "ERROR: " << msg << std::endl;

    parseContext.handleError( ParseContext::SUMMARY_UNKNOWN_WELL , msg );
}


void handleMissingGroup( const ParseContext& parseContext , const std::string& keyword, const std::string& group) {
    std::string msg = std::string("Error in keyword:") + keyword + std::string(" No such group: ") + group;
    if (parseContext.get( ParseContext::SUMMARY_UNKNOWN_GROUP) == InputError::WARN)
        std::cerr << "ERROR: " << msg << std::endl;

    parseContext.handleError( ParseContext::SUMMARY_UNKNOWN_GROUP , msg );
}

  inline void keywordW( SummaryConfig::keyword_list& list,
                      const ParseContext& parseContext,
                      const DeckKeyword& keyword,
                      const Schedule& schedule ) {

    const auto hasValue = []( const DeckKeyword& kw ) {
        return kw.getDataRecord().getDataItem().hasValue( 0 );
    };
    const std::array<int,3> dummy_dims = {1,1,1};

    if (keyword.size() && hasValue(keyword)) {
        for( const std::string& pattern : keyword.getStringData()) {
            auto wells = schedule.getWellsMatching( pattern );

            if( wells.empty() )
                handleMissingWell( parseContext, keyword.name(), pattern );

            for( const auto* well : wells )
                list.push_back( SummaryConfig::keyword_type( smspec_node_alloc( ECL_SMSPEC_WELL_VAR,
                                                                                well->name().c_str(),
                                                                                keyword.name().c_str(),
                                                                                "",
                                                                                ":",
                                                                                dummy_dims.data(),
                                                                                0,0,0)));
        }
    } else
        for (const auto* well : schedule.getWells())
            list.push_back( SummaryConfig::keyword_type( smspec_node_alloc( ECL_SMSPEC_WELL_VAR,
                                                                            well->name().c_str(),
                                                                            keyword.name().c_str(),
                                                                            "",
                                                                            ":",
                                                                            dummy_dims.data(),
                                                                            0,0,0)));
  }


inline void keywordG( SummaryConfig::keyword_list& list,
                      const ParseContext& parseContext,
                      const DeckKeyword& keyword,
                      const Schedule& schedule ) {

    const std::array<int,3> dummy_dims = {1,1,1};
    if( keyword.name() == "GMWSET" ) return;

    if( keyword.size() == 0 ||
        !keyword.getDataRecord().getDataItem().hasValue( 0 ) ) {

        for( const auto& group : schedule.getGroups() ) {
            if( group->name() == "FIELD" ) continue;
            list.push_back( SummaryConfig::keyword_type( smspec_node_alloc( ECL_SMSPEC_GROUP_VAR,
                                                                            group->name().c_str(),
                                                                            keyword.name().c_str(),
                                                                            "",
                                                                            ":",
                                                                            dummy_dims.data(),
                                                                            0, 0, 0)));
        }
        return;
    }

    const auto& item = keyword.getDataRecord().getDataItem();

    for( const std::string& group : item.getData< std::string >() ) {
        if( schedule.hasGroup( group ) )
            list.push_back( SummaryConfig::keyword_type( smspec_node_alloc( ECL_SMSPEC_GROUP_VAR,
                                                                            group.c_str(),
                                                                            keyword.name().c_str(),
                                                                            "",
                                                                            ":",
                                                                            dummy_dims.data(),
                                                                            0, 0, 0)));


        else
            handleMissingGroup( parseContext, keyword.name(), group );
    }
}

  inline void keywordF( SummaryConfig::keyword_list& list,
                        const DeckKeyword& keyword ) {
      const std::array<int,3> dummy_dims = {1,1,1};
      if( keyword.name() == "FMWSET" ) return;
      list.push_back( SummaryConfig::keyword_type( smspec_node_alloc( ECL_SMSPEC_FIELD_VAR,
                                                                      NULL,
                                                                      keyword.name().c_str(),
                                                                      "",
                                                                      ":",
                                                                      dummy_dims.data(),
                                                                      0, 0, 0)));

  }

inline std::array< int, 3 > getijk( const DeckRecord& record,
                                    int offset = 0 ) {
    return {{
        record.getItem( offset + 0 ).get< int >( 0 ) - 1,
        record.getItem( offset + 1 ).get< int >( 0 ) - 1,
        record.getItem( offset + 2 ).get< int >( 0 ) - 1
    }};
}

inline std::array< int, 3 > getijk( const Connection& completion ) {
    return { { completion.getI(), completion.getJ(), completion.getK() }};
}

  inline void keywordB( SummaryConfig::keyword_list& list,
                        const DeckKeyword& keyword,
                        const GridDims& dims) {
    for( const auto& record : keyword ) {
        auto ijk = getijk( record );
        int global_index = 1 + dims.getGlobalIndex(ijk[0], ijk[1], ijk[2]);
        list.push_back( SummaryConfig::keyword_type( smspec_node_alloc(ECL_SMSPEC_BLOCK_VAR,
                                                                       NULL,
                                                                       keyword.name().c_str(),
                                                                       "",
                                                                       ":",
                                                                       dims.getNXYZ().data(),
                                                                       global_index,0,0)));

    }
}

  inline void keywordR( SummaryConfig::keyword_list& list,
                      const DeckKeyword& keyword,
                      const TableManager& tables,
                      const GridDims& dims) {

    /* RUNSUM is not a region keyword but a directive for how to format and
     * print output. Unfortunately its *recognised* as a region keyword
     * because of its structure and position. Hence the special handling of ignoring it.
     */
    if( keyword.name() == "RUNSUM" ) return;
    if( keyword.name() == "RPTONLY" ) return;

    const size_t numfip = tables.numFIPRegions( );
    const auto& item = keyword.getDataRecord().getDataItem();
    std::vector<int> regions;

    if (item.size() > 0)
        regions = item.getData< int >();
    else {
        for (size_t region=1; region <= numfip; region++)
            regions.push_back( region );
    }

    for( const int region : regions ) {
        if (region >= 1 && region <= static_cast<int>(numfip))
            list.push_back( SummaryConfig::keyword_type( smspec_node_alloc( ECL_SMSPEC_REGION_VAR,
                                                                            NULL,
                                                                            keyword.name().c_str(),
                                                                            "",
                                                                            ":",
                                                                            dims.getNXYZ().data(),
                                                                            region,
                                                                            0, 0)));
        else
            throw std::invalid_argument("Illegal region value: " + std::to_string( region ));
    }
}


  inline void keywordMISC( SummaryConfig::keyword_list& list,
                           const DeckKeyword& keyword)
{
    const std::array<int,3> dummy_dims = {1,1,1};
    if (meta_keywords.count( keyword.name() ) == 0)
        list.push_back( SummaryConfig::keyword_type( smspec_node_alloc( ECL_SMSPEC_MISC_VAR,
                                                                        NULL,
                                                                        keyword.name().c_str(),
                                                                        "",
                                                                        ":",
                                                                        dummy_dims.data(),
                                                                        0,
                                                                        0, 0)));
}


  inline void keywordC( SummaryConfig::keyword_list& list,
                        const ParseContext& parseContext,
                        const DeckKeyword& keyword,
                        const Schedule& schedule,
                        const GridDims& dims) {

    const auto& keywordstring = keyword.name();
    const auto last_timestep = schedule.getTimeMap().last();

    for( const auto& record : keyword ) {

        const auto& wellitem = record.getItem( 0 );

        const auto wells = wellitem.defaultApplied( 0 )
                         ? schedule.getWells()
                         : schedule.getWellsMatching( wellitem.getTrimmedString( 0 ) );

        if( wells.empty() )
            handleMissingWell( parseContext, keyword.name(), wellitem.getTrimmedString( 0 ) );

        for( const auto* well : wells ) {
            const auto& name = well->name();

            /*
             * we don't want to add completions that don't exist, so we iterate
             * over a well's completions regardless of the desired block is
             * defaulted or not
             */
            for( const auto& connection : well->getConnections( last_timestep ) ) {
                /* block coordinates defaulted */
                auto cijk = getijk( connection );

                if( record.getItem( 1 ).defaultApplied( 0 ) ) {
                    int global_index = 1 + dims.getGlobalIndex(cijk[0], cijk[1], cijk[2]);
                    list.push_back( SummaryConfig::keyword_type( smspec_node_alloc( ECL_SMSPEC_COMPLETION_VAR,
                                                                                    name.c_str(),
                                                                                    keywordstring.c_str(),
                                                                                    "",
                                                                                    ":",
                                                                                    dims.getNXYZ().data(),
                                                                                    global_index,
                                                                                    0, 0)));
                } else {
                    /* block coordinates specified */
                    auto recijk = getijk( record, 1 );
                    if( std::equal( recijk.begin(), recijk.end(), cijk.begin() ) ) {
                        int global_index = 1 + dims.getGlobalIndex(recijk[0], recijk[1], recijk[2]);
                        list.push_back( SummaryConfig::keyword_type( smspec_node_alloc( ECL_SMSPEC_COMPLETION_VAR,
                                                                                        name.c_str(),
                                                                                        keywordstring.c_str(),
                                                                                        "",
                                                                                        ":",
                                                                                        dims.getNXYZ().data(),
                                                                                        global_index,
                                                                                        0, 0)));
                    }
                }
            }
        }
    }
}

    inline void keywordS(SummaryConfig::keyword_list& list,
                         const ParseContext&          parseContext,
                         const DeckKeyword&           keyword,
                         const Schedule&              schedule)
    {
        // Generate SMSPEC nodes for SUMMARY keywords of the form
        //
        //   SOFR
        //     'W1'   1 /
        //     'W1'  10 /
        //     'W3'     / -- All segments
        //   /
        //
        //   SPR
        //     1*   2 / -- Segment 2 in all multi-segmented wells
        //   /

        if (keyword.name() == "SUMMARY") {
            // The SUMMARY keyword itself invokes keywordS().  Ignore it.
            return;
        }

        // Modifies 'list' in place.
        auto makeNode = [&list, &keyword]
            (const std::string& well, const int segNumber) -> void
        {
            // Grid dimensions immaterial for segment related vectors.
            const int dims[] = { 1, 1, 1 };

            list.push_back(SummaryConfig::keyword_type {
                smspec_node_alloc(ECL_SMSPEC_SEGMENT_VAR, well.c_str(),
                                  keyword.name().c_str(), "", ":",
                                  dims, segNumber, 0, 0)
            });
        };

        const auto last_timestep = schedule.getTimeMap().last();

        for (const auto& record : keyword) {
            const auto& wellitem = record.getItem(0);
            const auto& wells    = wellitem.defaultApplied(0)
                ? schedule.getWells()
                : schedule.getWellsMatching(wellitem.getTrimmedString(0));

            if (wells.empty()) {
                handleMissingWell(parseContext, keyword.name(),
                                  wellitem.getTrimmedString(0));
            }

            for (const auto* well : wells) {
                if (! well->isMultiSegment(last_timestep)) {
                    // Not an MSW.  Don't create summary vectors for segments.
                    continue;
                }

                const auto& wname = well->name();
                const auto& segID = record.getItem(1);

                if (segID.defaultApplied(0)) {
                    // Segment number defaulted.  Allocate a summary
                    // vector for each segment.
                    const auto nSeg =
                        well->getWellSegments(last_timestep).size();

                    for (auto segNumber = 0*nSeg;
                              segNumber <   nSeg; ++segNumber)
                    {
                        makeNode(wname, segNumber + 1);  // One-based.
                    }
                }
                else {
                    // Segment number specified.  Allocate single
                    // summary vector for that segment number.
                    makeNode(wname, segID.get<int>(0));
                }
            }
        }
    }

  inline void handleKW( SummaryConfig::keyword_list& list,
                        const DeckKeyword& keyword,
                        const Schedule& schedule,
                        const TableManager& tables,
                        const ParseContext& parseContext,
                        const GridDims& dims) {
    const auto var_type = ecl_smspec_identify_var_type( keyword.name().c_str() );

    switch( var_type ) {
        case ECL_SMSPEC_WELL_VAR: return keywordW( list, parseContext, keyword, schedule );
        case ECL_SMSPEC_GROUP_VAR: return keywordG( list, parseContext, keyword, schedule );
        case ECL_SMSPEC_FIELD_VAR: return keywordF( list, keyword );
        case ECL_SMSPEC_BLOCK_VAR: return keywordB( list, keyword, dims );
        case ECL_SMSPEC_REGION_VAR: return keywordR( list, keyword, tables, dims );
        case ECL_SMSPEC_COMPLETION_VAR: return keywordC( list, parseContext, keyword, schedule, dims);
        case ECL_SMSPEC_SEGMENT_VAR: return keywordS( list, parseContext, keyword, schedule );
        case ECL_SMSPEC_MISC_VAR: return keywordMISC( list, keyword );

        default: return;
    }
}

  inline void uniq( SummaryConfig::keyword_list& vec ) {
    const auto lt = []( const SummaryConfig::keyword_type& lhs,
                        const SummaryConfig::keyword_type& rhs ) {
        return smspec_node_cmp(lhs.get(), rhs.get()) < 0;
    };

    const auto eq = []( const SummaryConfig::keyword_type& lhs,
                        const SummaryConfig::keyword_type& rhs ) {
        return smspec_node_equal(lhs.get(), rhs.get());
    };

    std::sort( vec.begin(), vec.end(), lt );
    auto logical_end = std::unique( vec.begin(), vec.end(), eq );
    vec.erase( logical_end, vec.end() );
  }
}

SummaryConfig::SummaryConfig( const Deck& deck,
                              const Schedule& schedule,
                              const TableManager& tables,
                              const ParseContext& parseContext,
                              const GridDims& dims) {
    SUMMARYSection section( deck );
    for( auto& x : section )
        handleKW( this->keywords, x, schedule, tables, parseContext, dims);

    if( section.hasKeyword( "ALL" ) )
        this->merge( { ALL_keywords, schedule, tables, parseContext, dims} );

    if( section.hasKeyword( "GMWSET" ) )
        this->merge( { GMWSET_keywords, schedule, tables, parseContext, dims} );

    if( section.hasKeyword( "FMWSET" ) )
        this->merge( { FMWSET_keywords, schedule, tables, parseContext, dims} );

    if (section.hasKeyword( "PERFORMA" ) )
        this->merge( { PERFORMA_keywords, schedule, tables, parseContext, dims} );

    uniq( this->keywords );
    for (const auto& kw: this->keywords) {
        this->short_keywords.insert( smspec_node_get_keyword( kw.get() ));
        this->summary_keywords.insert( smspec_node_get_gen_key1( kw.get() ));
    }

}

SummaryConfig::SummaryConfig( const Deck& deck,
                              const Schedule& schedule,
                              const TableManager& tables,
                              const ParseContext& parseContext) :
    SummaryConfig( deck , schedule, tables, parseContext, GridDims( deck ))
{

}

SummaryConfig::const_iterator SummaryConfig::begin() const {
    return this->keywords.cbegin();
}

SummaryConfig::const_iterator SummaryConfig::end() const {
    return this->keywords.cend();
}

SummaryConfig& SummaryConfig::merge( const SummaryConfig& other ) {
    this->keywords.insert( this->keywords.end(),
                            other.keywords.begin(),
                            other.keywords.end() );

    uniq( this->keywords );
    return *this;
}

SummaryConfig& SummaryConfig::merge( SummaryConfig&& other ) {
    auto fst = std::make_move_iterator( other.keywords.begin() );
    auto lst = std::make_move_iterator( other.keywords.end() );
    this->keywords.insert( this->keywords.end(), fst, lst );
    other.keywords.clear();

    uniq( this->keywords );
    return *this;
}


bool SummaryConfig::hasKeyword( const std::string& keyword ) const {
    return (this->short_keywords.count( keyword ) == 1);
}


bool SummaryConfig::hasSummaryKey(const std::string& keyword ) const {
    return (this->summary_keywords.count( keyword ) == 1);
}


/*
  Can be used to query if a certain 3D field, e.g. PRESSURE, is
  required to calculate the summary variables.

  The implementation is based on the hardcoded datastructure
  required_fields defined in a anonymous namespaces at the top of this
  file; the content of this datastructure again is based on the
  implementation of the Summary calculations in the opm-output
  repository: opm/output/eclipse/Summary.cpp.
*/

bool SummaryConfig::require3DField( const std::string& keyword ) const {
    const auto iter = required_fields.find( keyword );
    if (iter == required_fields.end())
        return false;

    for (const auto& kw : iter->second) {
        if (this->hasKeyword( kw ))
            return true;
    }

    return false;
}


bool SummaryConfig::requireFIPNUM( ) const {
    return this->hasKeyword("ROIP")  ||
           this->hasKeyword("ROIPL") ||
           this->hasKeyword("RGIP")  ||
           this->hasKeyword("RGIPL") ||
           this->hasKeyword("RGIPG") ||
           this->hasKeyword("RWIP")  ||
           this->hasKeyword("RPR");
}

}
