package org.likenes;

import android.content.Context;
import android.net.nsd.NsdManager;
import android.net.nsd.NsdServiceInfo;
import java.net.Inet6Address;
import java.net.InetAddress;
import java.util.ArrayDeque;
import java.util.HashMap;

/** Owns the Android NSD callbacks and hands a bounded event queue to the game thread. */
@SuppressWarnings("deprecation")
public final class DiscoveryBridge {
    private static final String TYPE = "_likenes._udp.";
    private static final int LIMIT = 16;
    private final NsdManager manager;
    private final ArrayDeque<String> events = new ArrayDeque<>();
    private final ArrayDeque<NsdServiceInfo> pending = new ArrayDeque<>();
    private final HashMap<String, NsdServiceInfo> found = new HashMap<>();
    private NsdManager.DiscoveryListener browser;
    private NsdManager.RegistrationListener registration;
    private boolean resolving;
    private boolean closed;
    private boolean failed;

    public DiscoveryBridge(Context context) {
        manager = (NsdManager) context.getSystemService(Context.NSD_SERVICE);
    }

    public synchronized boolean start(String name, int port) {
        if (manager == null || closed || browser != null || registration != null) return false;
        try {
            if (port == 0) {
                browser = new Browser();
                manager.discoverServices(TYPE, NsdManager.PROTOCOL_DNS_SD, browser);
            } else {
                registration = new Registration();
                NsdServiceInfo info = new NsdServiceInfo();
                info.setServiceName(name);
                info.setServiceType(TYPE);
                info.setPort(port);
                manager.registerService(info, NsdManager.PROTOCOL_DNS_SD, registration);
            }
            return true;
        } catch (RuntimeException ex) {
            close();
            return false;
        }
    }

    public synchronized String pop() {
        if (failed) return "!";
        return events.pollFirst();
    }

    public synchronized void close() {
        if (closed) return;
        closed = true;
        pending.clear();
        found.clear();
        events.clear();
        if (browser != null) {
            try { manager.stopServiceDiscovery(browser); } catch (RuntimeException ignored) {}
        }
        if (registration != null) {
            try { manager.unregisterService(registration); } catch (RuntimeException ignored) {}
        }
        browser = null;
        registration = null;
    }

    private synchronized void fail() {
        if (!closed) failed = true;
    }

    private void event(String value) {
        if (events.size() < LIMIT * 2) events.addLast(value);
        else fail();
    }

    private static boolean validName(String name) {
        return name != null && !name.isEmpty() && name.length() <= 63 &&
            name.indexOf('\t') < 0 && name.indexOf('\n') < 0 && name.indexOf('\r') < 0;
    }

    private synchronized void added(NsdServiceInfo info) {
        String name = info.getServiceName();
        if (closed || failed || !validName(name) || found.size() >= LIMIT ||
            found.putIfAbsent(name, info) != null) return;
        pending.addLast(info);
        resolveNext();
    }

    private synchronized void removed(NsdServiceInfo info) {
        String name = info.getServiceName();
        if (closed || found.remove(name) == null) return;
        pending.removeIf(p -> p.getServiceName().equals(name));
        event("-" + name);
    }

    private void resolveNext() {
        if (resolving || closed || failed || pending.isEmpty()) return;
        NsdServiceInfo next = pending.removeFirst();
        resolving = true;
        try {
            manager.resolveService(next, new Resolver(next));
        } catch (RuntimeException ex) {
            resolving = false;
            fail();
        }
    }

    private synchronized void resolved(NsdServiceInfo original, NsdServiceInfo info, boolean ok) {
        resolving = false;
        if (closed || failed) return;
        String name = original.getServiceName();
        if (found.get(name) != original) {
            resolveNext();
            return;
        }
        if (ok) {
            InetAddress host = info.getHost();
            if (host != null) {
                String address = host.getHostAddress();
                if (host instanceof Inet6Address) {
                    int zone = address.indexOf('%');
                    if (zone >= 0) address = address.substring(0, zone);
                    int scope = ((Inet6Address) host).getScopeId();
                    address = "[" + address + (scope == 0 ? "" : "%" + scope) + "]";
                }
                event("+" + name + "\t" + address + ":" + info.getPort());
            }
        } else {
            found.remove(name);
        }
        resolveNext();
    }

    private final class Browser implements NsdManager.DiscoveryListener {
        @Override public void onDiscoveryStarted(String type) {}
        @Override public void onDiscoveryStopped(String type) {}
        @Override public void onStartDiscoveryFailed(String type, int error) { fail(); }
        @Override public void onStopDiscoveryFailed(String type, int error) { fail(); }
        @Override public void onServiceFound(NsdServiceInfo service) { added(service); }
        @Override public void onServiceLost(NsdServiceInfo service) { removed(service); }
    }

    private final class Registration implements NsdManager.RegistrationListener {
        @Override public void onServiceRegistered(NsdServiceInfo info) {}
        @Override public void onServiceUnregistered(NsdServiceInfo info) {}
        @Override public void onRegistrationFailed(NsdServiceInfo info, int error) { fail(); }
        @Override public void onUnregistrationFailed(NsdServiceInfo info, int error) { fail(); }
    }

    private final class Resolver implements NsdManager.ResolveListener {
        private final NsdServiceInfo original;

        Resolver(NsdServiceInfo original) { this.original = original; }
        @Override public void onServiceResolved(NsdServiceInfo info) { resolved(original, info, true); }
        @Override public void onResolveFailed(NsdServiceInfo info, int error) { resolved(original, info, false); }
    }
}
