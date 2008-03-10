#ifndef ROOT_TreeSearch_Road
#define ROOT_TreeSearch_Road

///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// TreeSearch::Road                                                          //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "Hit.h"
#include "TVector2.h"
#include <set>
#include <utility>
#include <vector>
#include <list>
#include <functional>
#include <cassert>
#include <cstring>

namespace TreeSearch {

  class Projection;
  class NodeDescriptor;
  class Hit;
  class HitSet;
  class BuildInfo_t;    // Defined in implementation

  typedef std::pair<const NodeDescriptor,HitSet> Node_t;

  class Road : public TObject {

  public:
    // Coordinates of hit positions, for track fitting
    struct Point {
      Point() : x(0), hit(0) {}
      Point( Double_t _x, Double_t _z, Hit* _hit ) 
	: x(_x), z(_z), hit(_hit) { assert(hit); }
      Double_t res() const { return hit->GetResolution(); }
      Double_t x;    // Selected x coordinates
      Double_t z;    // z coordinate
      Hit*     hit;  // Associated hit (stored in WirePlane)
    };

    // Fit results
    struct FitResult {
      Double_t fPos, fSlope, fChi2, fV[3];
      std::vector<TreeSearch::Road::Point*>   fFitCoordinates;
      FitResult( Double_t pos, Double_t slope, Double_t chi2, Double_t* cov )
	: fPos(pos), fSlope(slope), fChi2(chi2)
      { assert(cov); memcpy(fV, cov, 3*sizeof(Double_t)); }
      FitResult() {}
      // Sort fit results by ascending chi2
      bool operator<( const FitResult& rhs ) const 
      { return ( fChi2 < rhs.fChi2 ); }
      const std::vector<TreeSearch::Road::Point*>& GetPoints() const 
      { return fFitCoordinates; }

      struct Chi2IsLess
	: public std::binary_function< FitResult*, FitResult*, bool >
      {
	bool operator() ( const FitResult* a, const FitResult* b ) const
	{ assert(a&&b); return ( a->fChi2 < b->fChi2 ); }
      };
    };

    // For global variable access/event display
    friend class Corners;
    class Corners : public TObject {
    public:
      explicit Corners( Road* rd ) 
	: fXLL(rd->fCornerX[0]), fXLR(rd->fCornerX[1]), fZL(rd->fZL), 
	  fXUL(rd->fCornerX[3]), fXUR(rd->fCornerX[2]), fZU(rd->fZU) {} 
      Corners() {}  // For ROOT RTTI
      virtual ~Corners() {}
    private:
      Double_t fXLL;  // Lower left corner x coord
      Double_t fXLR;  // Lower right corner x coord
      Double_t fZL;   // Lower edge z coord
      Double_t fXUL;  // Upper left corner x coord
      Double_t fXUR;  // Upper right corner x coord
      Double_t fZU;   // Upper edge z coord
      ClassDef(Corners,0)
    };

    explicit Road( const Projection* proj );
    Road() {} // For internal ROOT use
    Road( const Road& );
    Road& operator=( const Road& );
    virtual ~Road();

    Bool_t         Add( Node_t& nd );
    Bool_t         Adopt( const Road* other );
    virtual Int_t  Compare( const TObject* obj ) const;
    void           Finish();
    Bool_t         Fit();
    Double_t       GetChi2( UInt_t ifit=0 ) const;
    FitResult*     GetFitResult( UInt_t ifit=0 ) const;
    UInt_t         GetNfits() const { return (UInt_t)fFitData.size(); }
    Double_t       GetPos     ( Double_t z = 0 ) const;
    Double_t       GetPosErrsq( Double_t z = 0 ) const;
    const Projection* GetProjection() const { return fProjection; }
    Double_t       GetSlope() const { return fSlope; }
    Bool_t         Includes ( const Road* other ) const;
    TVector2       Intersect( const Road* other, Double_t z ) const;
    Bool_t         IsGood() const { return fGood; }
    virtual Bool_t IsSortable () const { return kTRUE; }
    Bool_t         IsVoid() const { return !fGood; }
    virtual void   Print( Option_t* opt="" ) const;
    void           Void() { fGood = false; }

  protected:

    const Projection*  fProjection; //! Projection that this Road belongs to

    Double_t           fCornerX[5]; // x positions of corners
    Double_t           fZL, fZU;    // z +/- eps of first/last plane 


    std::list<Node_t*> fPatterns;   // Patterns in this road
    Hset_t             fHits;       // All hits linked to the patterns
    
    std::vector< std::vector<Point*> > fPoints; // Hit pos within road
    std::vector<FitResult*>  fFitData; // Good fit results, sorted by chi2

    // Best fit results (copy of fFitData.begin() for global variable access)
    Double_t  fPos;      // Track origin
    Double_t  fSlope;    // Track slope
    Double_t  fChi2;     // Chi2 of fit
    Double_t  fV[3];     // Covariance matrix of param (V11, V12=V21, V22)
    UInt_t    fDof;      // Degrees of freedom of fit (nhits-2)

    Bool_t    fGood;     // Road successfully built and fit


    BuildInfo_t* fBuild; //! Working data for building

    Bool_t   CheckMatch( const Hset_t& hits ) const;
    Bool_t   CollectCoordinates();
    Double_t GetBinX( UInt_t bin ) const;

    ClassDef(Road,1)  // Region containing track candidate hits and fit results
  };

  //___________________________________________________________________________
  inline
  Int_t Road::Compare( const TObject* obj ) const 
  {
    // Used for sorting Roads in a TClonesArray or similar.
    // A Road is "less than" another if the chi2 of its best fit is smaller.
    // Returns -1 if this is smaller than rhs, 0 if equal, +1 if greater.

    // Require identical classes of objects
    assert( obj && IsA() == obj->IsA() );

    //TODO: take fDof into account & compare statistical significance
    if( fChi2 < static_cast<const Road*>(obj)->fChi2 ) return -1;
    if( fChi2 > static_cast<const Road*>(obj)->fChi2 ) return  1;
    return 0;
  }

  //---------------------------------------------------------------------------
  inline
  Double_t Road::GetChi2( UInt_t ifit ) const
  {
    // Return unnormalized chi2 of the i-th fit
    assert( ifit < GetNfits() );
    return fFitData[ifit]->fChi2;
  }

  //---------------------------------------------------------------------------
  inline
  Road::FitResult* Road::GetFitResult( UInt_t ifit ) const
  {
    // Return fit results of i-th fit
    assert( ifit < GetNfits() );
    return fFitData[ifit];
  }

  //---------------------------------------------------------------------------
  inline
  Double_t Road::GetPos( Double_t z ) const
  {
    // Return x = a1+a2*z for best fit (in m)
    
    return fPos + z*fSlope;
  }

  //---------------------------------------------------------------------------
  inline
  Double_t Road::GetPosErrsq( Double_t z ) const
  {
    // Return square of uncertainty in x = a1+z2*z for best fit (in m^2)
    
    return fV[0] + 2.0*fV[1]*z + fV[2]*z*z;
  }

///////////////////////////////////////////////////////////////////////////////

} // end namespace TreeSearch


#endif
