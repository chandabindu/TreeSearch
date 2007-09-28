///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// TreeSearch::PatternGenerator                                              //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "PatternGenerator.h"
#include "TMath.h"
#include "TString.h"
#include <iostream>
#include <fstream>
#include <sstream>
#if ROOT_VERSION_CODE < ROOT_VERSION(5,8,0)
#include <cstdlib>
#endif

//FIXME: TEST
#include <ctime>
//TODO: Print("C") = generate and count patterns
//TODO: Print("P") = generate and visualize patterns
//TODO: Interface to PatternTree
//TODO: Write() tree to binary file
//TODO: Read() tree from binary file

using namespace std;

ClassImp(TreeSearch::PatternGenerator)

namespace TreeSearch {

//_____________________________________________________________________________
inline
PatternGenerator::ChildIter& PatternGenerator::ChildIter::operator++()
{ 
  // Iterator over all suitable child patterns of a given parent pattern
  // that occur when the bin resolution is doubled.
  // Child pattern bits are either 2*bit or 2*bit+1 of the parent bits,
  // yielding 2^nbits (=2^nplanes) different combinations.
  // The bits of suitable patterns must monotonically increase.
  // bit[0] is always zero (otherwise the pattern could be shifted).
  // Note that type() is an important part of the result.
  // type & 1 indicates a pattern shifted by one to the right
  // type & 2 indicates a mirrored pattern 
  // To recover the actual pattern, mirror first, then shift, as appropriate.
  // With the self-referential tree structure used here, mirrored patterns
  // only ever occur as children of the root of the tree. Simultaneously
  // mirrored and shifted patterns never occur.
  // Hence, type = 0, 1, and, very rarely, 2.

  if( fCount > 0 ) {
    while( fCount-- ) {
      Int_t maxbit = 0;
      Int_t minbit = 1;
      UInt_t nbits = fChild.GetNbits();
      for( UInt_t ibit = nbits; ibit--; ) {
	Int_t bit = fParent[ibit] << 1;
	if( fCount & (1<<ibit) )
	  ++bit;
	fChild[ibit] = bit;
	if( bit < minbit ) minbit = bit;
	if( bit > maxbit ) maxbit = bit;
      }
      Int_t width = fChild.GetWidth();
      if( maxbit-minbit > TMath::Abs(width) )
	continue;
      if( minbit == 0 )
	fType = 0;
      else {
	fType = 1;
	for( UInt_t ibit = nbits; ibit; )
	  --fChild[--ibit];
      }
      if( width < 0 ) {
	fType += 2;
	width = -width;
	for( UInt_t ibit = nbits; ibit--; )
	  fChild[ibit] = width-fChild[ibit];
      }
      break;
    }
  } else
    fCount = -1;
  return *this;
}

//_____________________________________________________________________________
PatternGenerator::PatternGenerator() : fNlevels(0), fNplanes(0), fMaxSlope(0)
{
  // Constructor. 

}

//_____________________________________________________________________________
PatternGenerator::~PatternGenerator()
{
  // Destructor

  DoTree( kDelete );

}

//_____________________________________________________________________________
void PatternGenerator::DoTree( EOperation op )
{
  // Execute given operation on all unique Pattern nodes of the build tree.
  // Internal utility function.

  for( vector<Link*>::iterator it = fHashTable.begin();
       it != fHashTable.end(); ++it ) {
    Link* hashnode = *it;
    while( hashnode ) {
      Link* cur_node = hashnode;
      hashnode = hashnode->Next();
      assert( cur_node->GetPattern() );
      switch( op ) {
      case kDelete:
	delete cur_node->GetPattern();
	delete cur_node;
	break;
      case kResetRefIndex:
	cur_node->GetPattern()->fRefIndex = -1;
	break;
      }
    }
  }
  if( op == kDelete )
    fHashTable.clear();
}

//_____________________________________________________________________________
void PatternGenerator::CalcStatistics()
{
  // Collect statistics on the build tree. This is best done separately here
  // because some things (averages, memory requirements) can only be 
  // calculated once the tree is complete..

  memset( &fStats, 0, sizeof(Statistics_t) );

  for( vector<Link*>::const_iterator it = fHashTable.begin();
       it != fHashTable.end(); ++it ) {
    // Each hashnode points to a unique pattern by construction of the table
    Link* hashnode = *it;
    UInt_t hash_length = 0;
    while( hashnode ) {
      // Count patterns
      fStats.nPatterns++;
      // Count child nodes and length of child list
      Pattern* pat = hashnode->GetPattern();
      assert(pat);
      Link* ln = pat->fChild;
      UInt_t list_length = 0;
      while( ln ) {
	fStats.nLinks++;
	list_length++;
	ln = ln->Next();
      }
      if( list_length > fStats.MaxChildListLength )
	fStats.MaxChildListLength = list_length;
      // Count collision list depth
      hash_length++;

      hashnode = hashnode->Next();
    } // while hashnode

    if( hash_length > fStats.MaxHashDepth )
      fStats.MaxHashDepth = hash_length;

  } // hashtable elements

  fStats.nBytes = 
    fStats.nPatterns * sizeof(Pattern)
    + fStats.nPatterns * fNplanes * sizeof(UShort_t)
    + fStats.nLinks * sizeof(Link);
  fStats.nHashBytes = fHashTable.size() * sizeof(Link*)
    + fStats.nPatterns * sizeof(Link);
}

//_____________________________________________________________________________
void PatternGenerator::Print( Option_t* opt, ostream& os ) const
{
  // Print information about the tree, depending on option

  // Dump all nodes
  if( *opt == 'D' ) {
    for( vector<Link*>::const_iterator it = fHashTable.begin();
	 it != fHashTable.end(); ++it ) {
      Link* hashnode = *it;
      while( hashnode ) {
	Pattern* pat = hashnode->GetPattern();
	pat->Print( true, os );
	hashnode = hashnode->Next();
      }
    }
    return;
  }

  // Basic info
  os << "tree: nlevels = " << fNlevels
     << ", nplanes = " << fNplanes
     << ", zpos = ";
  for( UInt_t i=0; i<fZ.size(); i++ ) {
    os << fZ[i];
    if( i+1 != fZ.size() )
      os << ",";
  }
  os << endl;

  os << "patterns = " << fStats.nPatterns
     << ", links = "   << fStats.nLinks
     << ", bytes = " << fStats.nBytes
     << endl;
  os << "maxlinklen = " << fStats.MaxChildListLength
     << ", maxhash = " << fStats.MaxHashDepth
     << ", hashbytes = " << fStats.nHashBytes
     << endl;
  os << "time = " << fStats.BuildTime << " s" << endl;
 
 //TODO: add more features

}

//_____________________________________________________________________________
Int_t PatternGenerator::Write( const char* filename )
{
  // Write tree to binary file

  //  WalkTree( root, WritePattern(outfile) );

  return 0;
}

//_____________________________________________________________________________
void PatternGenerator::AddHash( Pattern* pat )
{
  // Add given pattern to the hash table

  assert(pat);
  UInt_t hashsize = fHashTable.size();
  if( hashsize == 0 ) {
    // Set the size of the hash table.
    // 2^(nlevels-1)*2^(nplanes-2) is the upper limit for the number of
    // patterns, so a size of 2^(nlevels-1) will give 2^(nplanes-2) collisions
    // per entry (i.e. 2, 4, 8), with which we can live. Anything better would
    // require a cleverer hash function.
    fHashTable.resize( 1<<(fNlevels-1), 0 );
    hashsize = fHashTable.size();
  }
  Int_t hash = pat->Hash()%hashsize;
  fHashTable[hash] = new Link( pat, fHashTable[hash], 0 );
}

//_____________________________________________________________________________
PatternTree* PatternGenerator::Generate( TreeParam_t parameters )
{
  // Generate a new pattern tree for the given parameters. Returns a pointer
  // to the generated tree, or zero if error

  // Set parameters for the new build.
  if( PatternTree::Normalize(parameters) != 0 )
    return 0;

  fNlevels  = parameters.maxdepth+1;
  fZ        = parameters.zpos;
  fNplanes  = fZ.size();
  fMaxSlope = parameters.maxslope;


  // Benchmark the build
  clock_t cpu_ticks = clock();

  // Start with the trivial all-zero root node at depth 0. 
  Pattern* root = new Pattern( fNplanes );
  AddHash( root );

  // Generate the tree recursively
  MakeChildNodes( root, 1 );
  
  // Calculate tree statistics (number of patterns, links etc.)
  cpu_ticks = clock() - cpu_ticks;
  CalcStatistics();
  fStats.BuildTime = static_cast<Double_t>(cpu_ticks)/CLOCKS_PER_SEC;

  //FIXME: TEST
  // Print tree statistics
  Print();
  // Dump the entire database for inspection
  ofstream outfile("nodes.txt");
  if( outfile ) {
    Print("D",outfile);
    outfile.close();
  }

  // Create a PatternTree object from the build tree

  PatternTree* tree = new PatternTree( parameters,
				       fStats.nPatterns,
				       fStats.nLinks );
  if( tree ) {
//     Int_t (PatternTree::*CopyPattern)(int) = &PatternTree::AddPattern;
//     (tree->*CopyPattern)(1);
    
//    WalkTree( root, CopyPattern(tree) );

  }
  return tree;
}

//_____________________________________________________________________________
PatternTree* PatternGenerator::Generate( UInt_t maxdepth, 
					 Double_t detector_width, 
					 const char* zpos,
					 Double_t maxslope )
{
  // Convenience function for use at the command line and in scripts

  TreeParam_t param;
  param.maxdepth = maxdepth;
  param.width    = detector_width;
  param.maxslope = maxslope;
  
  TString zlist( zpos );
  TString tok;
  Ssiz_t pos = 0;
  while( zlist.Tokenize(tok, pos, ",") ) {
    if( tok.IsNull() )
      continue;
#if ROOT_VERSION_CODE < ROOT_VERSION(5,8,0)
    Double_t val = atof(tok.Data());
#else
    Double_t val = tok.Atof();
#endif
    param.zpos.push_back( val );
  }

  return Generate( param );
}

//_____________________________________________________________________________
bool PatternGenerator::TestSlope( const Pattern& pat, UInt_t depth )
{
  UInt_t width = pat.GetWidth();
  return ( width < 2 ||
	   TMath::Abs((double)(width-1) / (double)(1<<depth)) <= fMaxSlope );
}

//_____________________________________________________________________________
bool PatternGenerator::LineCheck( const Pattern& pat )
{
  // Check if the gievn bit pattern is consistent with a straight line.
  // The intersection plane positions are given by fZ[].
  // Assumes a normalized pattern, for which pat[0] is always zero.
  // Assumes identical bin sizes and positions in each plane.

  // FIXME FIXME: for certain z-values, the following can be _very_ sensitive 
  // to the floating point rounding behavior!

  assert(fNplanes);
  Double_t xL   = pat[fNplanes-1];
  Double_t xRm1 = xL;               // xR-1
  Double_t zL   = fZ[fNplanes-1];
  Double_t zR   = zL;

  for( Int_t i = fNplanes-2; i > 0; --i ) {
    // Compare the intersection point with the i-th plane of the left edge 
    // of the band, (xL-x0) * z[i]/zL, to the left edge of the bin, pat[i]-x0. 
    // If the difference is equal or larger than one bin width (=1), the bin is
    // outside of the allowed band.
    // Multiply with zL (to avoid division) and recall x0 = 0.
    Double_t dL = xL*fZ[i] - pat[i]*zL;
    if( TMath::Abs(dL) >= zL )
      return false;
    // Likewise for the right edge
    Double_t dR = xRm1*fZ[i] - pat[i]*zR;
    if( TMath::Abs(dR) >= zR )
      return false;

    if( i > 1 ) {
      // If dL>0, the right edge of the bin is inside the band,
      // so set a new right-side limit.
      if( dL > 0 ) {
	xRm1 = pat[i];
	zR   = fZ[i];
      }
      // Likewise for the left-side limit
      if( dR < 0 ) {
	xL = pat[i];
	zL = fZ[i];
      }
    }
  } // planes
  return true;
}

//_____________________________________________________________________________
Pattern* PatternGenerator::Find( const Pattern& pat )
{
  // Search for the given pattern in the current database

  UInt_t hashsize = fHashTable.size();
  assert(hashsize);
  Int_t hash = pat.Hash()%hashsize;
  Link* link = fHashTable[hash];
  while( link ) {
    Pattern* rhs = link->GetPattern();
    if( pat == *rhs )
      return rhs;
    link = link->Next();
  }
  return 0;
}

//_____________________________________________________________________________
void PatternGenerator::MakeChildNodes( Pattern* parent, UInt_t depth )
{
  // Generate child nodes for the given parent pattern

  // Requesting child nodes for the parent at this depth implies that the 
  // parent is being used at the level above
  if( depth > 0 )
    parent->UsedAtDepth( depth-1 );

  // Base case of the recursion: no child nodes beyond fNlevels-1
  if( depth >= fNlevels )
    return;

  // If not already done, generate the child patterns of this parent
  if( !parent->fChild ) {
    ChildIter it( *parent );
    while( it ) {
      Pattern& child = *it;

      // Pattern already exists?
      Pattern* node = Find( child );
      if( node ) {
	// If the pattern has only been tested at a higher depth, we need to
	// redo the slope test since the slope is larger now at lower depth
	if( depth >= node->fMinDepth || TestSlope(*node, depth)) {
	  // Only add a reference to the existing pattern
	  parent->AddChild( node, it.type() );
	}
      } else if( TestSlope(child, depth) && LineCheck(child) ) {
	// If the pattern is new, check it for consistency with maxslope and 
	// the straight line condition.
	node = new Pattern( child );
	AddHash( node );
	parent->AddChild( node, it.type() );
      }
      ++it;
    }
  }

  // Recursively generate child nodes down the tree
  Link* ln = parent->GetChild();
  while( ln ) {
    Pattern* node = ln->GetPattern();
    // We only need to go deeper if either this pattern does not have children
    // yet OR (important!) children were previously generated from a deeper
    // location in the tree and so this pattern's subtree needs to be extended
    // deeper down now.
    if( !node->fChild || node->fMinDepth > depth )
      MakeChildNodes( node, depth+1 );
    ln = ln->Next();
  }
}

///////////////////////////////////////////////////////////////////////////////

} // end namespace TreeSearch