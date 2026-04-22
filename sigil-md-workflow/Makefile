PANDOC  = pandoc
SRC     = $(sort $(wildcard chapters/*.md))
OUT     = book.epub
META    = metadata.yaml
CSS     = style.css

$(OUT): $(SRC) $(META) $(CSS)
	$(PANDOC) $(SRC) -o $@ \
	  --metadata-file=$(META) \
	  --css=$(CSS) \
	  --toc \
	  --toc-depth=2 \
	  --split-level=1

clean:
	rm -f $(OUT)

.PHONY: clean
