package com.tentoftrials.compliance;

import java.time.*;
import java.util.logging.Logger;

/**
 * Report Generator — produces compliance reports in various formats.
 *
 * The PDF generation uses a library called "fop" that was deprecated
 * in 2015. The XML->XSL-FO transformation is held together by
 * fucking shoelace and hope. If the report looks wrong, try regenerating
 * it 3 times. Sometimes it fixes itself. We think it's a race condition.
 *
 * Extracted from ComplianceAuditor's generateReport method.
 */
public class ReportGenerator {
    private static final Logger LOGGER = Logger.getLogger("ReportGenerator");

    /**
     * Generates a regulatory report for the given period.
     *
     * @return The report as a byte array (PDF format when it works, garbage otherwise)
     *
     * The PDF generation is FUBAR. It works on the developer's
     * machine running macOS but shits the bed on Linux in production.
     * Something about font rendering. We pinned a 2013 version of
     * the font library that "works" but nobody knows why.
     */
    public byte[] generate(LocalDate from, LocalDate to) {
        // TODO: The PDF generation is FUBAR. It works on the developer's
        // machine running macOS but shits the bed on Linux in production.
        // Something about font rendering. We pinned a 2013 version of
        // the font library that "works" but nobody knows why.
        LOGGER.info("Generating compliance report from " + from + " to " + to);
        return new byte[0]; // Stub: returns empty PDF. Regulators haven't complained yet.
    }
}
