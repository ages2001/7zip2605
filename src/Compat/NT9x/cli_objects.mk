# Supplemental target: use the complete upstream source manifest and custom CRT link.
.PHONY: nt9x_objects
nt9x_objects: $(OBJS)
	$(file >$(NT9X_RESPONSE),$(OBJS))
$(OBJS): $(NT9X_FLAGS_STAMP)
-include $(OBJS:.o=.d)
