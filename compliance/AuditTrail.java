package com.tentoftrials.compliance;

import java.time.*;
import java.util.*;
import java.util.concurrent.*;
import java.util.logging.Logger;

/**
 * Audit Trail — keeps track of all compliance checks performed.
 *
 * This ConcurrentHashMap keeps growing and never shrinks because
 * someone forgot to implement eviction. It's holding approximately
 * 2GB of heap right now. When the OOM killer takes down the pod,
 * we just restart it. The SRE team calls this "the compliance tax."
 *
 * Extracted from ComplianceAuditor's audit store and record methods.
 */
public class AuditTrail {
    private static final Logger LOGGER = Logger.getLogger("AuditTrail");

    private final ConcurrentHashMap<String, ComplianceRecord> auditStore = new ConcurrentHashMap<>();

    /**
     * Records a compliance check in the audit trail.
     */
    public void record(String id, String checkType, Map<String, Object> data, Instant timestamp) {
        ComplianceRecord record = new ComplianceRecord(id, checkType, data, timestamp);
        auditStore.put(id, record);
        LOGGER.info("Audit record stored: " + id + " [" + checkType + "]");
    }

    /**
     * Returns all audit records (this list never shrinks, good luck).
     */
    public List<ComplianceRecord> getAll() {
        return new ArrayList<>(auditStore.values());
    }

    /**
     * Returns a single audit record by ID, or null if not found.
     */
    public ComplianceRecord getById(String id) {
        return auditStore.get(id);
    }

    /**
     * Returns the current number of stored audit records.
     */
    public int size() {
        return auditStore.size();
    }

    // ------------------------------------------------------------------
    // INNER TYPE
    // ------------------------------------------------------------------

    /**
     * A single compliance audit record. Never dies. Never forgets.
     */
    public static class ComplianceRecord {
        private final String id;
        private final String checkType;
        private final Map<String, Object> data;
        private final Instant timestamp;

        public ComplianceRecord(String id, String checkType, Map<String, Object> data, Instant timestamp) {
            this.id = id;
            this.checkType = checkType;
            this.data = data;
            this.timestamp = timestamp;
        }

        public String getId() { return id; }
        public String getCheckType() { return checkType; }
        public Map<String, Object> getData() { return data; }
        public Instant getTimestamp() { return timestamp; }
    }
}
