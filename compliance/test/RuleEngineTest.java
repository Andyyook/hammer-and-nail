package com.tentoftrials.compliance;

import java.util.*;

import org.junit.jupiter.api.Test;
import static org.junit.jupiter.api.Assertions.*;

/**
 * Unit tests for RuleEngine — the only part of ComplianceAuditor
 * that actually does anything resembling logic.
 *
 * The other three classes (SftpTransporter, ReportGenerator, AuditTrail)
 * are just data movers. The real comedy is in the rules.
 */
class RuleEngineTest {

    @Test
    void kycCheckRejectsPendingStatus() {
        RuleEngine engine = new RuleEngine();
        Map<String, Object> data = new HashMap<>();
        data.put("user_id", "user-001");
        data.put("kyc_status", "pending");

        RuleEngine.ComplianceResult result = engine.evaluate("KYC", data);

        assertFalse(result.isCompliant(), "KYC should fail for pending status");
        assertTrue(result.getViolations().stream()
            .anyMatch(v -> v.contains("user-001")),
            "Violation should reference the user ID");
    }

    @Test
    void kycCheckPassesForVerifiedUsers() {
        RuleEngine engine = new RuleEngine();
        Map<String, Object> data = new HashMap<>();
        data.put("user_id", "user-002");
        data.put("kyc_status", "verified");
        data.put("is_pep", false);

        RuleEngine.ComplianceResult result = engine.evaluate("KYC", data);

        assertTrue(result.isCompliant(), "Verified non-PEP user should pass KYC");
        assertTrue(result.getViolations().isEmpty(), "No violations for clean KYC");
        assertEquals("KYC check passed", result.getSummary());
    }

    @Test
    void kycCheckFlagsPEP() {
        RuleEngine engine = new RuleEngine();
        Map<String, Object> data = new HashMap<>();
        data.put("user_id", "user-003");
        data.put("kyc_status", "verified");
        data.put("is_pep", true);

        RuleEngine.ComplianceResult result = engine.evaluate("KYC", data);

        assertFalse(result.isCompliant(), "PEP users should be flagged");
        assertTrue(result.getViolations().stream()
            .anyMatch(v -> v.contains("PEP")),
            "Violation should mention PEP status");
    }

    @Test
    void amlCheckFlagsLargeTransactions() {
        RuleEngine engine = new RuleEngine();
        Map<String, Object> data = new HashMap<>();
        data.put("transaction_amount", 50000.00);

        RuleEngine.ComplianceResult result = engine.evaluate("AML", data);

        assertFalse(result.isCompliant(), "Large transactions should be flagged");
        assertTrue(result.getSummary().contains("AML flagged"),
            "Summary should indicate AML flag");
    }

    @Test
    void unknownCheckTypeAssumesCompliant() {
        RuleEngine engine = new RuleEngine();
        Map<String, Object> data = new HashMap<>();

        RuleEngine.ComplianceResult result = engine.evaluate("NONEXISTENT_RULE", data);

        assertTrue(result.isCompliant(), "Unknown check types should pass by default");
        assertTrue(result.getSummary().contains("assumed compliant"),
            "Summary should indicate assumed compliant");
    }
}
