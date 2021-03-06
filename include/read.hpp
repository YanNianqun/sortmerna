#pragma once
/**
* FILE: read.hpp
* Created: Nov 06, 2017 Mon
* Wrapper of a Reads' record and its Match results
*/

#include <string>
#include <sstream>
#include <vector>
#include <algorithm> // std::find_if

#include "kvdb.hpp"
#include "traverse_bursttrie.hpp" // id_win
#include "ssw.hpp" // s_align2
#include "options.hpp"

class References; // forward

struct alignment_struct2
{
	uint32_t max_size; // max size of alignv i.e. max number of alignments to store (see options '-N', '--best N') TODO: remove?
	uint32_t min_index; // index into alignv for the reference with lowest SW alignment score (see 'compute_lis_alignment')
	uint32_t max_index; // index into alignv for the reference with highest SW alignment score (see 'compute_lis_alignment')
	std::vector<s_align2> alignv;

	alignment_struct2() : max_size(0), min_index(0), max_index(0) {}

	alignment_struct2(std::string); // create from binary string

	std::string toString(); // convert to binary string
	size_t getSize() { 
		size_t ret = sizeof(min_index) + sizeof(max_index);
		for (std::vector<s_align2>::iterator it = alignv.begin(); it != alignv.end(); ++it)
			ret += it->size();
		return ret;
	}

	void clear()
	{
		max_size = 0;
		min_index = 0;
		max_index = 0;
		alignv.clear();
	}
};

class Read {
public:
	unsigned int id; // number of the read in the reads file. Store in Database.
	bool isValid; // flags the record is not valid
	bool isEmpty; // flags the Read object is empty i.e. just a placeholder for copy assignment
	bool is03; // indicates Read::isequence is in 0..3 alphabet
	bool is04; // indicates Read:iseqeunce is in 0..4 alphabet. Seed search cannot proceed on 0-4 alphabet
	bool isRestored; // flags the read is restored from Database. See 'Read::restoreFromDb'

	std::string header;
	std::string sequence;
	std::string quality; // "" (fasta) | "xxx..." (fastq)
	Format format; // fasta | fastq

	// calculated
	std::string isequence; // sequence in Integer alphabet: [A,C,G,T] -> [0,1,2,3]
	bool reversed = false; // indicates the read is reverse-complement i.e. 'revIntStr' was applied
	std::vector<int> ambiguous_nt; // positions of ambiguous nucleotides in the sequence (as defined in nt_table/load_index.cpp)

	// store in database ------------>
	unsigned int lastIndex; // last index number this read was aligned against. Set in Processor::callback
	unsigned int lastPart; // last part number this read was aligned against.  Set in Processor::callback
	// matching results
	bool hit = false; // indicates that a match for this Read has been found
	bool hit_denovo = true; // hit & !(%Cov & %ID) TODO: change this to 'hit_cov_id' because it's set to true if !(%Cov & %ID) regardless of 'hit'
	bool null_align_output = false; // flags NULL alignment was output to file (needs to be done once only)
	uint16_t max_SW_count = 0; // count of matches that have Max Smith-Waterman score for this read
	int32_t num_alignments = 0; // number of alignments to output per read
	uint32_t readhit = 0; // number of seeds matches between read and database. (? readhit == id_win_hits.size)
	int32_t best = 0; // init with opts.min_lis, see 'this.init'. Don't DB store/restore (bug 51).

	// array of positions of window hits on the reference sequence in given index/part. 
	// Only used during alignment on a particular index/part. No need to store on disk.
	// Reset on each new index part
	// [0] : {id = 568 win = 0 	}
	// ...
	// [4] : {id = 1248788 win = 72 }
	//        |            |_k-mer start position on read
	//        |_k-mer id on reference (index into 'positions_tbl')
	std::vector<id_win> id_win_hits;

	alignment_struct2 hits_align_info; // stored in DB

	std::vector<int8_t> scoring_matrix; // initScoringMatrix   orig: int8_t* scoring_matrix
	// <------------------------------ store in database

	Read()
		:
		id(0),
		isValid(false),
		isEmpty(true),
		is03(false),
		is04(false),
		isRestored(false),
		lastIndex(0),
		lastPart(0)
	{
		//if (opts.num_alignments > 0) num_alignments = opts.num_alignments;
		//if (min_lis_gv > 0) best = min_lis_gv;
	}

	Read(int id, std::string header, std::string sequence, std::string quality, Format format)
		:
		id(id), header(std::move(header)), sequence(sequence),
		quality(quality), format(format), isEmpty(false)
	{
		validate();
		//seqToIntStr();
		//		initScoringMatrix(opts);
	}

	~Read() {
		//		if (hits_align_info.ptr != 0) {
		//	delete[] read.hits_align_info.ptr->cigar;
		//	delete read.hits_align_info.ptr;
		//			delete hits_align_info.ptr; 
		//		}
		//		free(scoring_matrix);
		//		scoring_matrix = 0;
	}

	// copy constructor
	Read(const Read & that)
	{
		id = that.id;
		isValid = that.isValid;
		isEmpty = that.isEmpty;
		is03 = that.is03;
		is04 = that.is04;
		isRestored = that.isRestored;
		header = that.header;
		sequence = that.sequence;
		quality = that.quality;
		format = that.format;
		isequence = that.isequence;
		reversed = that.reversed;
		ambiguous_nt = that.ambiguous_nt;
		lastIndex = that.lastIndex;
		lastPart = that.lastPart;
		hit = that.hit;
		hit_denovo = that.hit_denovo;
		null_align_output = that.null_align_output;
		max_SW_count = that.max_SW_count;
		num_alignments = that.num_alignments;
		readhit = that.readhit;
		best = that.best;
		id_win_hits = that.id_win_hits;
		hits_align_info = that.hits_align_info;
		scoring_matrix = that.scoring_matrix;
	}

	// copy assignment
	Read & operator=(const Read & that)
	{
		if (&that == this) return *this;

		//printf("Read copy assignment called\n");
		id = that.id;
		isValid = that.isValid;
		isEmpty = that.isEmpty;
		is03 = that.is03;
		is04 = that.is04;
		isRestored = that.isRestored;
		header = that.header;
		sequence = that.sequence;
		quality = that.quality;
		format = that.format;
		isequence = that.isequence;
		reversed = that.reversed;
		ambiguous_nt = that.ambiguous_nt;
		lastIndex = that.lastIndex;
		lastPart = that.lastPart;
		hit = that.hit;
		hit_denovo = that.hit_denovo;
		null_align_output = that.null_align_output;
		max_SW_count = that.max_SW_count;
		num_alignments = that.num_alignments;
		readhit = that.readhit;
		best = that.best;
		id_win_hits = that.id_win_hits;
		hits_align_info = that.hits_align_info;
		scoring_matrix = that.scoring_matrix;

		return *this; // by convention always return *this
	}

	void initScoringMatrix(Runopts & opts);

	// convert char "sequence" to 0..3 alphabet "isequence", and populate "ambiguous_nt"
	void seqToIntStr() 
	{
		for (std::string::iterator it = sequence.begin(); it != sequence.end(); ++it)
		{
			char c = nt_table[(int)*it];
			if (c == 4) // ambiguous nt. 4 is max value in nt_table
			{
				ambiguous_nt.push_back(static_cast<int>(isequence.size())); // i.e. add current position to the vector
				c = 0;
			}
			isequence += c;
		}
		is03 = true;
	}

	// reverse complement the integer sequence in 03 encoding
	void revIntStr() {
		std::reverse(isequence.begin(), isequence.end());
		for (int i = 0; i < isequence.length(); i++) {
			isequence[i] = complement[(int)isequence[i]];
		}
		reversed = !reversed;
	}

	// convert isequence to alphabetic form i.e. to A,C,G,T,N
	std::string get04alphaSeq() {
		//bool rev03 = false; // mark whether to revert back to 03
		std::string seq;
		if (is03) flip34();
		// convert to alphabetic
		for (int i = 0; i < isequence.size(); ++i)
			seq += nt_map[(int)isequence[i]];

		//if (rev03) flip34();
		return seq;
	}

	void validate() {
		std::stringstream ss;
		if (sequence.size() > READLEN)
		{
			ss << std::endl << RED << "ERROR" << COLOFF << ": [" << __FILE__ << ":"<< __LINE__ 
				<< "] Read ID: " << id << " Header: " << header << " Sequence length: " << sequence.size() << " > "  << READLEN << " nt " << std::endl
				<< "  Please check your reads or contact the authors." << std::endl;
			std::cerr << ss.str();
			exit(EXIT_FAILURE);
		}
		isValid = true;
	} // ~validate

	void clear()
	{
		id = 0;
		isValid = false;
		isEmpty = true;
		is03 = false;
		is04 = false;
		header.clear();
		sequence.clear();
		quality.clear();
		isequence.clear();
		reversed = false;
		ambiguous_nt.clear();
		isRestored = false;
		lastIndex = 0;
		lastPart = 0;
		hit = false;
		hit_denovo = true;
		null_align_output = false;
		max_SW_count = 0;
		num_alignments = 0;
		readhit = 0;
		best = 0;
		id_win_hits.clear();
		hits_align_info.clear();
		scoring_matrix.clear();
	}

	void init(Runopts & opts, KeyValueDatabase & kvdb, unsigned int readId)
	{
		id = readId;
		if (opts.num_alignments > 0) num_alignments = opts.num_alignments;
		if (opts.min_lis > 0) best = opts.min_lis;
		validate();
		seqToIntStr();
		//unmarshallJson(kvdb); // get matches from Key-value database
		restoreFromDb(kvdb); // get matches from Key-value database
		initScoringMatrix(opts);
	}

	std::string matchesToJson(); // convert to Json string to store in DB

	std::string toString(); // convert to binary string to store in DB

	  // deserialize matches from string
	bool restoreFromDb(KeyValueDatabase & kvdb);

	// deserialize matches from JSON and populate the read
	void unmarshallJson(KeyValueDatabase & kvdb);

	// flip isequence between 03 - 04 alphabets
	void flip34()
	{
		if (ambiguous_nt.size() > 0) 
		{
			int val = is03 ? 4 : 0;
			if (reversed)
			{
				for (uint32_t p = 0; p < ambiguous_nt.size(); p++)
				{
					isequence[(isequence.length() - ambiguous_nt[p]) - 1] = val;
				}
			}
			else
			{
				for (uint32_t p = 0; p < ambiguous_nt.size(); p++)
				{
					isequence[ambiguous_nt[p]] = val;
				}
			}
			is03 = !is03;
			is04 = !is04;
		}
	} // ~flip34

	void calcMismatchGapId(References & refs, int alignIdx, uint32_t & mismatches, uint32_t & gaps, uint32_t & id);
	std::string getSeqId() {
		// part of the header from start till first space.
		std::string id = header.substr(0, header.find(' '));
		// remove '>' or '@'
		id.erase(id.begin(), std::find_if(id.begin(), id.end(), [](auto ch) {return !(ch == FASTA_HEADER_START || ch == FASTQ_HEADER_START);}));
		return id;
	}

	uint32_t hashKmer(uint32_t pos, uint32_t len);
}; // ~class Read
