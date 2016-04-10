SHELL = /bin/sh

NAME=mpiv
ALLFILES=$(shell ls *.tex)

all: $(NAME).pdf

LATEX_FILES := $(NAME).tex \
	$(NAME).bib

$(NAME).pdf: $(LATEX_FILES) $(ALLFILES)
	pdflatex $(NAME)
	bibtex $(NAME)
	pdflatex $(NAME)
	pdflatex $(NAME)

clean:
	if (rm -f *.log *.aux *.bbl *.blg *.toc *.dvi *.flc *~ $(NAME).pdf) then :; fi
