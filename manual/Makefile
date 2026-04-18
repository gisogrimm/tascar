SHELL=/bin/bash

include ../config.mk

all: acousticmodel.pdf manual.pdf callgraph.pdf

diff: manual-diff_master.pdf

TEXINPUTS = "./:../doc/:"

SECTIONS = secmodules.tex secactmodules.tex secapmodules.tex	\
secrecgeneric.tex secrecspeaker.tex secrecreverb.tex secmaskmodules.tex

%.run1: %.tex $(SECTIONS) autodoc.txt
	TEXINPUTS=$(TEXINPUTS) pdflatex -draftmode -halt-on-error -file-line-error $< && touch $@

%.run2: %.tex %.run1
	TEXINPUTS=$(TEXINPUTS) bibtex $(<:.tex=) && touch $@

%.run3: %.tex %.run2
	TEXINPUTS=$(TEXINPUTS) pdflatex -halt-on-error -file-line-error $< && touch $@

%.run4: %.tex %.run3
	TEXINPUTS=$(TEXINPUTS) makeindex $(basename $<) && touch $@

%.run5: %.tex %.run4
	TEXINPUTS=$(TEXINPUTS) pdflatex -halt-on-error -file-line-error $< > $@.err && touch $@ && ( ! grep -e 'LaTeX Warning: Reference.* undefined' $@.err)

%.aux: %.tex
	TEXINPUTS=$(TEXINPUTS) pdflatex $<

FIGURES = filestructure.pdf creator.pdf
#ap_sndfile.pdf

ALLAPPS := $(sort $(subst .o,,$(subst .mk,,$(wildcard ../apps/build/tascar_*))))

NOHELPAPPS = jackpar lsljacktime sendosc compare_level_sum	\
	test_compare_sndfile tscupdate test_compare_level_sum

ALLAPPNAMES := $(filter-out $(NOHELPAPPS),$(patsubst ../apps/build/tascar_%,%,$(ALLAPPS)))


version.tex:
	echo $(FULLVERSION) > $@

autodoc.txt: documentation.tsc $(wildcard ../*/build/*)
	rm -f tab*.tex;LD_LIBRARY_PATH=../libtascar/build/:../plugins/build/ DYLD_LIBRARY_PATH=../libtascar/build/:../plugins/build/ ../apps/build/tascar_validatetsc -l -g $< > $@
#&& cat $@ || (cat $@; false)

checkusetab: autodoc.txt
	for i in tab*.tex; do echo "\input{$$i}"; grep -q -e $$i *.tex || exit 1; done

checkundoc: checkusetab
	if grep -e undocumented tab*.tex; then false; else true; fi

manual.pdf: clihelp.tex version.tex $(FIGURES) $(SECTIONS) acousticmodel.pdf callgraph.pdf manual.run5

secmodules.tex: $(sort $(wildcard mod*.tex))
	echo 'List of general purpose modules:\par\begin{itemize}' > $@;\
	echo $(patsubst %,\\item \\indmod{%},$(subst mod,,$(subst .tex,,$^))) >> $@;\
	echo '\end{itemize}' >> $@;\
	echo "" >> $@;\
	(for m in $(subst mod,,$(subst .tex,,$^)); do echo "\\subsection{$${m/_/\\_}}\\label{sec:$$m}\\index{$${m/_/\\_} (module)}";echo "\\input{mod$$m.tex}"; done) >> $@

secactmodules.tex: $(sort $(wildcard actmod*.tex))
	echo 'List of actor modules:\par\begin{itemize}' > $@;\
	echo $(patsubst %,\\item \\indmod{%},$(subst actmod,,$(subst .tex,,$^))) >> $@;\
	echo '\end{itemize}' >> $@;\
	echo "" >> $@;\
	(for m in $(subst actmod,,$(subst .tex,,$^)); do echo "\\subsection{$${m/_/\\_}}\\label{sec:$$m}\\index{$${m/_/\\_} (actor module)}";echo "\\input{actmod$$m.tex}"; done) >> $@

secapmodules.tex: $(sort $(wildcard apmod*.tex))
	echo 'List of audio plugins:\par\begin{itemize}' > $@;\
	(for m in $(subst apmod,,$(subst .tex,,$^)); do echo "\\item \\indapmod[$$m]{$${m/_/\\_}}"; done) >> $@;\
	echo '\end{itemize}' >> $@;\
	echo "" >> $@;\
	echo "" >> $@;\
	(for m in $(subst apmod,,$(subst .tex,,$^)); do echo "\\subsection{$${m/_/\\_}}\\label{sec:ap_$$m}\\index{$${m/_/\\_} (audio plugin)}";echo "\\input{apmod$$m.tex}"; done) >> $@

secmaskmodules.tex: $(sort $(wildcard maskmod*.tex))
	echo 'List of mask plugins:\par\begin{itemize}' > $@;\
	(for m in $(subst maskmod,,$(subst .tex,,$^)); do echo "\\item \\indmaskmod[$$m]{$${m/_/\\_}}"; done) >> $@;\
	echo '\end{itemize}' >> $@;\
	echo "" >> $@;\
	echo "" >> $@;\
	(for m in $(subst maskmod,,$(subst .tex,,$^)); do echo "\\subsection{$${m/_/\\_}}\\label{sec:maskplug_$$m}\\index{$${m/_/\\_} (mask plugin)}";echo "\\input{maskmod$$m.tex}"; done) >> $@

secrecgeneric.tex: $(sort $(wildcard recgen*.tex))
	echo 'List of generic receiver types:\par\begin{itemize}' > $@;\
	(for m in $(subst recgen,,$(subst .tex,,$^)); do mm=$$(echo $$m|sed 's/_/\\_/g'); echo "\\item \\indrecgen[$$m]{$${mm}}"; done) >> $@;\
	echo '\end{itemize}' >> $@;\
	echo "" >> $@;\
	echo "" >> $@;\
	(for m in $(subst recgen,,$(subst .tex,,$^)); do mm=$$(echo $$m|sed 's/_/\\_/g'); echo "\\subsubsection{$${mm}}\\label{sec:recgen_$$m}\\index{$${mm} (receiver type)}";echo "\\input{recgen$$m.tex}"; done) >> $@

secrecspeaker.tex: $(sort $(wildcard recspk*.tex))
	echo 'List of speaker based receiver types:\par\begin{itemize}' > $@;\
	(for m in $(subst recspk,,$(subst .tex,,$^)); do echo "\\item \\indrecspk[$$m]{$${m/_/\\_}}"; done) >> $@;\
	echo '\end{itemize}' >> $@;\
	echo "" >> $@;\
	echo "" >> $@;\
	(for m in $(subst recspk,,$(subst .tex,,$^)); do echo "\\subsubsection{$${m/_/\\_}}\\label{sec:recspk_$$m}\\index{$${m/_/\\_} (speaker based receiver type)}";echo "\\input{recspk$$m.tex}"; done) >> $@

secrecreverb.tex: $(sort $(wildcard recrev*.tex))
	echo 'List of reverb receiver types:\par\begin{itemize}' > $@;\
	(for m in $(subst recrev,,$(subst .tex,,$^)); do echo "\\item \\indrecrev[$$m]{$${m/_/\\_}}"; done) >> $@;\
	echo '\end{itemize}' >> $@;\
	echo "" >> $@;\
	echo "" >> $@;\
	(for m in $(subst recrev,,$(subst .tex,,$^)); do echo "\\subsubsection{$${m/_/\\_}}\\label{sec:recrev_$$m}\\index{$${m/_/\\_} (reverb receiver type)}";echo "\\input{recrev$$m.tex}"; done) >> $@

%.pdf: %.eps
	epstopdf $< -o $@

%.pdf: %.dot
	dot -Tpdf < $< > $@

clean:
	rm -Rf *.log *.lot *.lof *.aux *.dep *.dvi *.toc *~ *.blg *.bbl *.brf manual.pdf plug_*.tex *.idx *.ilg *.out *.ind $(FIGURES) $(SECTIONS) version.tex *.run*
	$(MAKE) -C ./examples clean

%.png: %.dot
	dot -Tpng < $< > $@

fdp_%.png: %.dot
	cat $< | sed 's/subgraph[^{]*//1' | fdp -Tpng > $@

fdp_%.pdf: %.dot
	cat $< | sed 's/subgraph[^{]*//1' | fdp -Tpdf > $@

manual-diff%.tex: manual.tex manual$(*).tex
	latexdiff -c ld.cfg --exclude-safecmd="sum" --exclude-textcmd="section" --exclude-textcmd="subsection" --exclude-textcmd="subsubsection"  manual$(*).tex $< > $@

%.pdf: %.svg
	inkscape --export-pdf="$@" $<

.PHONY : manual_master.tex

manual_master.tex:
	rm -f manual_master.tex;\
	mv manual.tex manual_temp.tex;\
	git checkout master manual.tex;\
	mv manual.tex $@;\
	mv manual_temp.tex manual.tex

MODTEX=$(wildcard mod*.tex) $(wildcard actmod*.tex) $(wildcard apmod*.tex) $(wildcard maskmod*.tex)

testundocumented: $(MODTEX:.tex=.documented)

%.documented: %.tex
	@test 0 != `cat $< | wc -w` || (echo "$<:0:"; false)

clihelp.tex: $(patsubst %,cli_%.tex,$(ALLAPPNAMES)) Makefile
	(echo '\section{Command line interfaces}';\
	echo 'All command line applications of \tascar{} start with the prefix \verb!tascar_!.';\
	echo ' To get a list of valid command line options, use the flag \verb!-h! or \verb!--help!.')>$@;\
	for app in $(ALLAPPNAMES); do (\
	texapp=$$(echo $${app}|sed 's/_/\\_/g');\
	echo "\\subsection{tascar\_$${texapp}}";\
	echo "\\index{tascar\_$${texapp}}";\
	echo '';\
	test -e clihelp_$${app}.tex && echo "\\input{clihelp_$${app}.tex}";\
	echo '';\
	echo "\\input{cli_$${app}.tex}";\
	echo '';\
	)>>$@; done;\



cli_%.tex: ../apps/build/tascar_% Makefile
	(echo ''; echo '\begin{lstlisting}[numbers=none]'; LD_LIBRARY_PATH=../libtascar/build/:../plugins/build/ DYLD_LIBRARY_PATH=../libtascar/build/:../plugins/build/ $< -h && echo '\end{lstlisting}') > $@


modtabs:
	jackd -d dummy & PID=$$!; sleep 1;(export LD_LIBRARY_PATH="../libtascar/build/:../plugins/build/";export TASCARGENDOC="true";../gui/build/tascar documentation.tsc);kill -9 $$PID; mkdir -p modtabtex; cp tab*.tex modtabtex/

showundoc:
	for i in oscdoc_*.tex; do grep -q -e "input.$${i}" *.tex || echo "\input{$${i}}"; done
