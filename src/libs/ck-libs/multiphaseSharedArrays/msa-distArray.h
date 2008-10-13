// emacs mode line -*- mode: c++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*-
#ifndef MSA_DISTARRAY_H
#define MSA_DISTARRAY_H

#include "msa-DistPageMgr.h"


struct MSA_InvalidHandle { };

/**
   The MSA1D class is a handle to a distributed shared array of items
   of data type ENTRY. There are nEntries total numer of ENTRY's, with
   ENTRIES_PER_PAGE data items per "page".  It is implemented as a
   Chare Array of pages, and a Group representing the local cache.

   The requirements for the templates are:
     ENTRY: User data class stored in the array, with at least:
        - A default constructor and destructor
        - A working assignment operator
        - A working pup routine
     ENTRY_OPS_CLASS: Used to combine values for "accumulate":
        - A method named "getIdentity", taking no arguments and
          returning an ENTRY to use before any accumulation.
        - A method named "accumulate", taking a source/dest ENTRY by reference
          and an ENTRY to add to it by value or const reference.
     ENTRIES_PER_PAGE: Optional integer number of ENTRY objects
        to store and communicate at once.  For good performance,
        make sure this value is a power of two.
 */
template<class ENTRY, class ENTRY_OPS_CLASS, unsigned int ENTRIES_PER_PAGE=MSA_DEFAULT_ENTRIES_PER_PAGE>
class MSA1D
{
public:
    typedef MSA_CacheGroup<ENTRY, ENTRY_OPS_CLASS, ENTRIES_PER_PAGE> CacheGroup_t;
    typedef CProxy_MSA_CacheGroup<ENTRY, ENTRY_OPS_CLASS, ENTRIES_PER_PAGE> CProxy_CacheGroup_t;
    typedef CProxy_MSA_PageArray<ENTRY, ENTRY_OPS_CLASS, ENTRIES_PER_PAGE> CProxy_PageArray_t;
    typedef MSA1D<ENTRY, ENTRY_OPS_CLASS, ENTRIES_PER_PAGE> curMSA1D;

    // Forward-declare so that things returning them can be friended
    class Read; class Write; class Accum;

	class MSA1D_Handle
	{
    public:
        inline unsigned int length() const { return msa.length(); }

	protected:
        curMSA1D &msa;
        bool valid;

        friend Read  &curMSA1D::syncToRead (MSA1D_Handle &m, int single = DEFAULT_SYNC_SINGLE);
        friend Write &curMSA1D::syncToWrite(MSA1D_Handle &m, int single = DEFAULT_SYNC_SINGLE);
        friend Accum &curMSA1D::syncToAccum(MSA1D_Handle &m, int single = DEFAULT_SYNC_SINGLE);
        void inline checkInvalidate(curMSA1D *m) 
        {
            if (m != &msa || !valid)
                throw MSA_InvalidHandle();
            valid = false;
        }

        MSA1D_Handle(curMSA1D &msa_) 
            : msa(msa_), valid(true) 
        { }
        void checkValid()
        {
            if (!valid)
                throw MSA_InvalidHandle();
        }

    private:
        // Disallow copy construction
        MSA1D_Handle(MSA1D_Handle &m) {}
    };

    class Read : public MSA1D_Handle
    {
    protected:
        friend Read &curMSA1D::syncToRead(MSA1D_Handle &, int);
        Read(curMSA1D &msa_)
            :  MSA1D_Handle(msa_) { }
        using MSA1D_Handle::checkValid;
        using MSA1D_Handle::checkInvalidate;

    public:
        inline const ENTRY& get(unsigned int idx)
        {
            checkValid();
            return MSA1D_Handle::msa.get(idx); 
        }
        inline const ENTRY& operator[](unsigned int idx) { return get(idx); }
        inline const ENTRY& get2(unsigned int idx)
        {
            checkValid();
            return MSA1D_Handle::msa.get2(idx);
        }
    };

    class Write : public MSA1D_Handle
    {
    protected:
        friend Write &curMSA1D::syncToWrite(MSA1D_Handle &, int);
        friend Write &curMSA1D::getInitialWrite();
        Write(curMSA1D &msa_)
            : MSA1D_Handle(msa_) { }

    public:
        inline ENTRY& set(unsigned int idx)
        {
            MSA1D_Handle::checkValid();
            return MSA1D_Handle::msa.set(idx);
        }
    };

    class Accum : public MSA1D_Handle
    {
    protected:
        friend Accum &curMSA1D::syncToAccum(MSA1D_Handle &, int);
        friend Accum &curMSA1D::getInitialAccum();
        Accum(curMSA1D &msa_)
            : MSA1D_Handle(msa_) { }
        using MSA1D_Handle::checkInvalidate;
    public:
        inline ENTRY& accumulate(unsigned int idx)
        { 
            MSA1D_Handle::checkValid();
            return MSA1D_Handle::msa.accumulate(idx);
        }
        inline void accumulate(unsigned int idx, const ENTRY& ent)
        {
            MSA1D_Handle::checkValid();
            MSA1D_Handle::msa.accumulate(idx, ent);
        }
    };

protected:
    /// Total number of ENTRY's in the whole array.
    unsigned int nEntries;
    bool initHandleGiven;

    /// Handle to owner of cache.
    CacheGroup_t* cache;
    CProxy_CacheGroup_t cg;

    inline const ENTRY* readablePage(unsigned int page)
    {
        return (const ENTRY*)(cache->readablePage(page));
    }

    // known local page.
    inline const ENTRY* readablePage2(unsigned int page)
    {
        return (const ENTRY*)(cache->readablePage2(page));
    }

    // Returns a pointer to the start of the local copy in the cache of the writeable page.
    // @@ what if begin - end span across two or more pages?
    inline ENTRY* writeablePage(unsigned int page, unsigned int offset)
    {
        return (ENTRY*)(cache->writeablePage(page, offset));
    }

public:
    // @@ Needed for Jade
    inline MSA1D(){}
    virtual void pup(PUP::er &p){
        p|nEntries;
        p|cg;
        if (p.isUnpacking()) cache=cg.ckLocalBranch();
    }

    /**
      Create a completely new MSA array.  This call creates the
      corresponding groups, so only call it once per array.
    */
    inline MSA1D(unsigned int nEntries_, unsigned int num_wrkrs, 
                 unsigned int maxBytes=MSA_DEFAULT_MAX_BYTES) 
        : nEntries(nEntries_), initHandleGiven(false)
    {
        // first create the Page Array and the Page Group
        unsigned int nPages = (nEntries + ENTRIES_PER_PAGE - 1)/ENTRIES_PER_PAGE;
        CProxy_PageArray_t pageArray = CProxy_PageArray_t::ckNew(nPages);
        cg = CProxy_CacheGroup_t::ckNew(nPages, pageArray, maxBytes, nEntries, num_wrkrs);
        pageArray.setCacheProxy(cg);
        pageArray.ckSetReductionClient(new CkCallback(CkIndex_MSA_CacheGroup<ENTRY, ENTRY_OPS_CLASS, ENTRIES_PER_PAGE>::SyncDone(), cg));
        cache = cg.ckLocalBranch();
    }

    // Deprecated API for accessing CacheGroup directly.
    inline MSA1D(CProxy_CacheGroup_t cg_) : cg(cg_), initHandleGiven(false)
    {
        cache = cg.ckLocalBranch();
        nEntries = cache->getNumEntries();
    }

    inline ~MSA1D()
    {
        // TODO: how to get rid of the cache group and the page array
        //(cache->getArray()).destroy();
        //cg.destroy();
        // TODO: calling FreeMem does not seem to work. Need to debug it.
				cache->unroll();
 //       cache->FreeMem();
    }

    /**
     * this function is supposed to be called when the thread/object using this array
     * migrates to another PE.
     */
    inline void changePE()
    {
        cache = cg.ckLocalBranch();

        /* don't need to update the number of entries, as that does not change */
    }

    // ================ Accessor/Utility functions ================
    /// Get the total length of the array, across all processors.
    inline unsigned int length() const { return nEntries; }

    inline const CProxy_CacheGroup_t &getCacheGroup() const { return cg; }

    // Avoid using the term "page size" because it is confusing: does
    // it mean in bytes or number of entries?
    inline unsigned int getNumEntriesPerPage() const { return ENTRIES_PER_PAGE; }

    /// Return the page this entry is stored at.
    inline unsigned int getPageIndex(unsigned int idx)
    {
        return idx / ENTRIES_PER_PAGE;
    }

    /// Return the offset, in entries, that this entry is stored at within a page.
    inline unsigned int getOffsetWithinPage(unsigned int idx)
    {
        return idx % ENTRIES_PER_PAGE;
    }

    // ================ MSA API ================

    // We need to know the total number of workers across all
    // processors, and we also calculate the number of worker threads
    // running on this processor.
    //
    // Blocking method, basically does a barrier until all workers
    // enroll.
    inline void enroll(int num_workers)
    {
        // @@ This is a hack to identify the number of MSA1D
        // threads on this processor.  This number is needed for sync.
        //
        // @@ What if a MSA1D thread migrates?
        cache->enroll(num_workers);
    }

    // idx is the element to be read/written
    //
    // This function returns a reference to the first element on the
    // page that contains idx.
    inline ENTRY& getPageBottom(unsigned int idx, MSA_Page_Fault_t accessMode)
    {
        if (accessMode==Read_Fault) {
            unsigned int page = idx / ENTRIES_PER_PAGE;
            return const_cast<ENTRY&>(readablePage(page)[0]);
        } else {
            CkAssert(accessMode==Write_Fault || accessMode==Accumulate_Fault);
            unsigned int page = idx / ENTRIES_PER_PAGE;
            unsigned int offset = idx % ENTRIES_PER_PAGE;
            ENTRY* e=writeablePage(page, offset);
            return e[0];
        }
    }

    inline void FreeMem()
    {
        cache->FreeMem();
    }

    /// Non-blocking prefetch of entries from start to end, inclusive.
    /// Prefetch'd pages are locked into the cache, so you must call
    ///   unlock afterwards.
    inline void Prefetch(unsigned int start, unsigned int end)
    {
        unsigned int page1 = start / ENTRIES_PER_PAGE;
        unsigned int page2 = end / ENTRIES_PER_PAGE;
        cache->Prefetch(page1, page2);
    }

    /// Block until all prefetched pages arrive.
    inline int WaitAll()    { return cache->WaitAll(); }

    /// Unlock all locked pages
    inline void Unlock()    { return cache->UnlockPages(); }

    /// start and end are element indexes.
    /// Unlocks completely spanned pages given a range of elements
    /// index'd from "start" to "end", inclusive.  If start/end does not span a
    /// page completely, i.e. start/end is in the middle of a page,
    /// the entire page is still unlocked--in particular, this means
    /// you should not have several adjacent ranges locked.
    inline void Unlock(unsigned int start, unsigned int end)
    {
        unsigned int page1 = start / ENTRIES_PER_PAGE;
        unsigned int page2 = end / ENTRIES_PER_PAGE;
        cache->UnlockPages(page1, page2);
    }

    static const int DEFAULT_SYNC_SINGLE = 0;

    inline Read &syncToRead(MSA1D_Handle &m, int single = DEFAULT_SYNC_SINGLE)
    {
        m.checkInvalidate(this);
        delete &m;
        sync(single);
        return *(new Read(*this));
    }

    inline Write &syncToWrite(MSA1D_Handle &m, int single = DEFAULT_SYNC_SINGLE)
    {
        m.checkInvalidate(this);
        delete &m;
        sync(single);
        return *(new Write(*this));
    }

    inline Accum &syncToAccum(MSA1D_Handle &m, int single = DEFAULT_SYNC_SINGLE)
    {
        m.checkInvalidate(this);
        delete &m;
        sync(single);
        return *(new Accum(*this));
    }

    inline Write &getInitialWrite()
    {
        if (initHandleGiven)
            throw MSA_InvalidHandle();

        Write *w = new Write(*this);
        sync();
        initHandleGiven = true;
        return *w;
    }

    inline Accum &getInitialAccum()
    {
        if (initHandleGiven)
            throw MSA_InvalidHandle();

        Accum *a = new Accum(*this);
        sync();
        initHandleGiven = true;
        return *a;
    }

  // These are the meat of the MSA API, but they are only accessible
  // through appropriate handles (defined in the public section above).
protected:
    /// Return a read-only copy of the element at idx.
    ///   May block if the element is not already in the cache.
    inline const ENTRY& get(unsigned int idx)
    {
        unsigned int page = idx / ENTRIES_PER_PAGE;
        unsigned int offset = idx % ENTRIES_PER_PAGE;
        return readablePage(page)[offset];
    }

    inline const ENTRY& operator[](unsigned int idx)
    {
        return get(idx);
    }

    /// Return a read-only copy of the element at idx;
    ///   ONLY WORKS WHEN ELEMENT IS ALREADY IN THE CACHE--
    ///   WILL SEGFAULT IF ELEMENT NOT ALREADY PRESENT.
    ///    Never blocks; may crash if element not already present.
    inline const ENTRY& get2(unsigned int idx)
    {
        unsigned int page = idx / ENTRIES_PER_PAGE;
        unsigned int offset = idx % ENTRIES_PER_PAGE;
        return readablePage2(page)[offset];
    }

    /// Return a writeable copy of the element at idx.
    ///    Never blocks; will create a new blank element if none exists locally.
    ///    UNDEFINED if two threads set the same element.
    inline ENTRY& set(unsigned int idx)
    {
        unsigned int page = idx / ENTRIES_PER_PAGE;
        unsigned int offset = idx % ENTRIES_PER_PAGE;
        ENTRY* e=writeablePage(page, offset);
        return e[offset];
    }

    /// Fetch the ENTRY at idx to be accumulated.
    ///   You must perform the accumulation on 
    ///     the return value before calling "sync".
    ///   Never blocks.
    inline ENTRY& accumulate(unsigned int idx)
    {
        unsigned int page = idx / ENTRIES_PER_PAGE;
        unsigned int offset = idx % ENTRIES_PER_PAGE;
        return cache->accumulate(page, offset);
    }
    
    /// Add ent to the element at idx.
    ///   Never blocks.
    ///   Merges together accumulates from different threads.
    inline void accumulate(unsigned int idx, const ENTRY& ent)
    {
        accumulate(idx)+=ent;
    }

    /// Synchronize reads and writes across the entire array.
    inline void sync(int single=0)
    {
        cache->SyncReq(single); 
    }
};


// define a 2d distributed array based on the 1D array, support row major and column
// major arrangement of data
template<class ENTRY, class ENTRY_OPS_CLASS, unsigned int ENTRIES_PER_PAGE=MSA_DEFAULT_ENTRIES_PER_PAGE, MSA_Array_Layout_t ARRAY_LAYOUT=MSA_ROW_MAJOR>
class MSA2D : public MSA1D<ENTRY, ENTRY_OPS_CLASS, ENTRIES_PER_PAGE>
{
public:
    typedef CProxy_MSA_CacheGroup<ENTRY, ENTRY_OPS_CLASS, ENTRIES_PER_PAGE> CProxy_CacheGroup_t;
    typedef MSA1D<ENTRY, ENTRY_OPS_CLASS, ENTRIES_PER_PAGE> super;
    typedef MSA2D<ENTRY, ENTRY_OPS_CLASS, ENTRIES_PER_PAGE, ARRAY_LAYOUT> curMSA2D;

protected:
    unsigned int rows, cols;

public:
    // @@ Needed for Jade
    inline MSA2D() : super() {}
    virtual void pup(PUP::er &p) {
       super::pup(p);
       p|rows; p|cols;
    };

    class Read; class Write; class Accum;

	class MSA2D_Handle
	{
	protected:
        curMSA2D &msa;
        bool valid;

        friend Read  &curMSA2D::syncToRead (MSA2D_Handle&, int);
        friend Write &curMSA2D::syncToWrite(MSA2D_Handle&, int);
        friend Accum &curMSA2D::syncToAccum(MSA2D_Handle&, int);
        inline void checkInvalidate(curMSA2D *m)
        {
            if (&msa != m || !valid)
                throw MSA_InvalidHandle();
            valid = false;
        }

        MSA2D_Handle(curMSA2D &msa_) 
            : msa(msa_), valid(true) 
        { }
        inline void checkValid()
        {
            if (!valid)
                throw MSA_InvalidHandle();
        }
    private:
        // Disallow copy construction
        MSA2D_Handle(MSA2D_Handle &m) {}
    };

    class Read : public MSA2D_Handle
    {
    private:
        friend Read &curMSA2D::syncToRead(MSA2D_Handle &, int);
        Read(curMSA2D &msa_)
            :  MSA2D_Handle(msa_) { }

    public: 
        inline const ENTRY& get(unsigned int row, unsigned int col)
        {
            MSA2D_Handle::checkValid();
            return MSA2D_Handle::msa.get(row, col);
        }
        inline const ENTRY& get2(unsigned int row, unsigned int col)
        {
            MSA2D_Handle::checkValid();
            return MSA2D_Handle::msa.get2(row, col);
        }
    };

    class Write : public MSA2D_Handle
    {
    private:
        friend Write &curMSA2D::syncToWrite(MSA2D_Handle &, int);
        friend Write &curMSA2D::getInitialWrite();
       Write(curMSA2D &msa_)
            :  MSA2D_Handle(msa_) { }

    public: 
        inline ENTRY& set(unsigned int row, unsigned int col)
        {
            MSA2D_Handle::checkValid();
            return MSA2D_Handle::msa.set(row, col);
        }
    };

    inline MSA2D(unsigned int rows_, unsigned int cols_, unsigned int numwrkrs,
                 unsigned int maxBytes=MSA_DEFAULT_MAX_BYTES)
        :super(rows_*cols_, numwrkrs, maxBytes)
    {
        rows = rows_; cols = cols_;
    }

    inline MSA2D(unsigned int rows_, unsigned int cols_, CProxy_CacheGroup_t cg_)
        : rows(rows_), cols(cols_), super(cg_)
    {}

    // get the 1D index of the given entry as per the row major/column major format
    inline unsigned int getIndex(unsigned int row, unsigned int col)
    {
        unsigned int index;

        if(ARRAY_LAYOUT==MSA_ROW_MAJOR)
            index = row*cols + col;
        else
            index = col*rows + row;

        return index;
    }

    // Which page is (row, col) on?
    inline unsigned int getPageIndex(unsigned int row, unsigned int col)
    {
        return getIndex(row, col)/ENTRIES_PER_PAGE;
    }

    inline unsigned int getOffsetWithinPage(unsigned int row, unsigned int col)
    {
        return getIndex(row, col)%ENTRIES_PER_PAGE;
    }

    inline unsigned int getRows(void) const {return rows;}
    inline unsigned int getCols(void) const {return cols;}
    inline unsigned int getColumns(void) const {return cols;}
    inline MSA_Array_Layout_t getArrayLayout() const {return ARRAY_LAYOUT;}

    inline void Prefetch(unsigned int start, unsigned int end)
    {
        // prefetch the start ... end rows/columns into the cache
        if(start > end)
        {
            unsigned int temp = start;
            start = end;
            end = temp;
        }

        unsigned int index1 = (ARRAY_LAYOUT==MSA_ROW_MAJOR) ? getIndex(start, 0) : getIndex(0, start);
        unsigned int index2 = (ARRAY_LAYOUT==MSA_ROW_MAJOR) ? getIndex(end, cols-1) : getIndex(rows-1, end);

        MSA1D<ENTRY, ENTRY_OPS_CLASS, ENTRIES_PER_PAGE>::Prefetch(index1, index2);
    }

    // Unlocks pages starting from row "start" through row "end", inclusive
    inline void UnlockPages(unsigned int start, unsigned int end)
    {
        if(start > end)
        {
            unsigned int temp = start;
            start = end;
            end = temp;
        }

        unsigned int index1 = (ARRAY_LAYOUT==MSA_ROW_MAJOR) ? getIndex(start, 0) : getIndex(0, start);
        unsigned int index2 = (ARRAY_LAYOUT==MSA_ROW_MAJOR) ? getIndex(end, cols-1) : getIndex(rows-1, end);

        MSA1D<ENTRY, ENTRY_OPS_CLASS, ENTRIES_PER_PAGE>::Unlock(index1, index2);
    }

    inline Read& syncToRead(MSA2D_Handle &m, int single = super::DEFAULT_SYNC_SINGLE)
    {
        m.checkInvalidate(this);
        delete &m;
        super::sync(single);
        return *(new Read(*this));
    }

    inline Write& syncToWrite(MSA2D_Handle &m, int single = super::DEFAULT_SYNC_SINGLE)
    {
        m.checkInvalidate(this);
        delete &m;
        super::sync(single);
        return *(new Write(*this));
    }

    inline Write& getInitialWrite()
    {
        if (super::initHandleGiven)
            throw MSA_InvalidHandle();

        Write *w = new Write(*this);
        super::initHandleGiven = true;
        return *w;
    }

protected:
    inline const ENTRY& get(unsigned int row, unsigned int col)
    {
        return super::get(getIndex(row, col));
    }

    // known local
    inline const ENTRY& get2(unsigned int row, unsigned int col)
    {
        return super::get2(getIndex(row, col));
    }

    // MSA2D::
    inline ENTRY& set(unsigned int row, unsigned int col)
    {
        return super::set(getIndex(row, col));
    }
};

#endif
