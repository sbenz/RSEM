/*
 * Assume any read should have a name other than ""
 */
#include<cstdio>
#include<cstring>
#include<cstdlib>
#include<cassert>
#include<iostream>
#include<fstream>
#include<string>

#include "utils.h"

#include "GroupInfo.h"

#include "SingleRead.h"
#include "SingleReadQ.h"
#include "PairedEndRead.h"
#include "PairedEndReadQ.h"
#include "SingleHit.h"
#include "PairedEndHit.h"

#include "HitContainer.h"
#include "SamParser.h"

using namespace std;

int read_type; // 0 SingleRead, 1 SingleReadQ, 2 PairedEndRead, 3 PairedEndReadQ
int N[3]; // note, N = N0 + N1 + N2 , but may not be equal to the total number of reads in data
int nHits; // # of hits
int nUnique, nMulti, nIsoMulti;
char fn_list[STRLEN];
char groupF[STRLEN];
char datF[STRLEN], cntF[STRLEN];

GroupInfo gi;

SamParser *parser;
ofstream hit_out;

int n_os; // number of ostreams
ostream *cat[3][2]; // cat : category  1-dim 0 N0 1 N1 2 N2; 2-dim  0 mate1 1 mate2
char readOutFs[3][2][STRLEN];

void init(const char* imdName, char alignFType, const char* alignF) {

	sprintf(datF, "%s.dat", imdName);
	sprintf(cntF, "%s.cnt", imdName);

	char* aux = 0;
	if (strcmp(fn_list, "")) aux = fn_list;
	parser = new SamParser(alignFType, alignF, aux);

	memset(cat, 0, sizeof(cat));
	memset(readOutFs, 0, sizeof(readOutFs));

	int tmp_n_os = -1;

	for (int i = 0; i < 3; i++) {
		genReadFileNames(imdName, i, read_type, n_os, readOutFs[i]);

		assert(tmp_n_os < 0 || tmp_n_os == n_os); tmp_n_os = n_os;

		for (int j = 0; j < n_os; j++)
			cat[i][j] = new ofstream(readOutFs[i][j]);
	}
}

//Do not allow duplicate for unalignable reads and supressed reads in SAM input
template<class ReadType, class HitType>
void parseIt(SamParser *parser) {
	// record_val & record_read are copies of val & read for record purpose
	int val, record_val;
	ReadType read, record_read;
	HitType hit;
	HitContainer<HitType> hits;

	nHits = 0;
	nUnique = nMulti = nIsoMulti = 0;
	memset(N, 0, sizeof(N));

	long long cnt = 0;

	record_val = -2; //indicate no recorded read now
	while ((val = parser->parseNext(read, hit)) >= 0) {
		if (val >= 0 && val <= 2) {
			// flush out previous read's info if needed
			if (record_val >= 0) {
				record_read.write(n_os, cat[record_val]);
				++N[record_val];
				hits.updateRI();
				nHits += hits.getNHits();
				nMulti += hits.calcNumGeneMultiReads(gi);
				nIsoMulti += hits.calcNumIsoformMultiReads();
				hits.write(hit_out);
			}

			hits.clear();
			record_val = val;
			record_read = read; // no pointer, thus safe
		}

		if (val == 1 || val == 5) {
			hits.push_back(hit);
		}

		++cnt;
		if (verbose && (cnt % 1000000 == 0)) { printf("Parsed %lld entries\n", cnt); }
	}

	if (record_val >= 0) {
		record_read.write(n_os, cat[record_val]);
		++N[record_val];
		hits.updateRI();
		nHits += hits.getNHits();
		nMulti += hits.calcNumGeneMultiReads(gi);
		nIsoMulti += hits.calcNumIsoformMultiReads();
		hits.write(hit_out);
	}

	nUnique = N[1] - nMulti;
}

void release() {
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < n_os; j++) {
			((ofstream*)cat[i][j])->close();
			delete cat[i][j];
		}
		if (N[i] > 0) continue;
		for (int j = 0; j < n_os; j++) {
			remove(readOutFs[i][j]); //delete if the file is empty
		}
	}
	delete parser;
}

int main(int argc, char* argv[]) {
	bool quiet = false;

	if (argc < 5) {
		printf("Usage : rsem-parse-alignments refName imdName alignFType('s' for sam, 'b' for bam) alignF [-t Type] [-l fn_list] [-tag tagName] [-q]\n");
		exit(-1);
	}

	strcpy(fn_list, "");
	read_type = 0;
	if (argc > 5) {
		for (int i = 5; i < argc; i++) {
			if (!strcmp(argv[i], "-t")) {
				read_type = atoi(argv[i + 1]);
			}
			if (!strcmp(argv[i], "-l")) {
				strcpy(fn_list, argv[i + 1]);
			}
			if (!strcmp(argv[i], "-tag")) {
				SamParser::setReadTypeTag(argv[i + 1]);
			}
			if (!strcmp(argv[i], "-q")) { quiet = true; }
		}
	}

	verbose = !quiet;

	init(argv[2], argv[3][0], argv[4]);

	sprintf(groupF, "%s.grp", argv[1]);
	gi.load(groupF);

	hit_out.open(datF);

	string firstLine(59, ' ');
	firstLine.append(1, '\n');		//May be dangerous!
	hit_out<<firstLine;

	switch(read_type) {
	case 0 : parseIt<SingleRead, SingleHit>(parser); break;
	case 1 : parseIt<SingleReadQ, SingleHit>(parser); break;
	case 2 : parseIt<PairedEndRead, PairedEndHit>(parser); break;
	case 3 : parseIt<PairedEndReadQ, PairedEndHit>(parser); break;
	}

	hit_out.seekp(0, ios_base::beg);
	hit_out<<N[1]<<" "<<nHits<<" "<<read_type;

	hit_out.close();

	//cntF for statistics of alignments file
	ofstream fout(cntF);
	fout<<N[0]<<" "<<N[1]<<" "<<N[2]<<" "<<(N[0] + N[1] + N[2])<<endl;
	fout<<nUnique<<" "<<nMulti<<" "<<nIsoMulti<<endl;
	fout<<nHits<<" "<<read_type<<endl;
	fout.close();

	release();

	if (verbose) { printf("Done!\n"); }

	return 0;
}