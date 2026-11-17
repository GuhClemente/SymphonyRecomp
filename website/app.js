// ==========================================================================
// SymphonyRecomp - Interactive App Logic, Radar Simulator & GA4 Analytics
// ==========================================================================

// Global safe GA4 event tracking helper
function trackEvent(eventName, params = {}) {
    if (typeof window.gtag === 'function') {
        window.gtag('event', eventName, {
            ...params,
            page_title: document.title,
            page_location: window.location.href,
            page_path: window.location.pathname
        });
    }
}

document.addEventListener('DOMContentLoaded', () => {
    initRadarSimulator();
    initShortcutSearch();
    initNavbarScroll();
    initAnalyticsTrackers();
    initCharacterRoster();
});

// 1. Interactive Co-op Radar Simulator (Canvas)
function initRadarSimulator() {
    const canvas = document.getElementById('radarCanvas');
    if (!canvas) return;
    const ctx = canvas.getContext('2d');

    const cols = 48;
    const rows = 24;
    const tileW = canvas.width / cols;
    const tileH = canvas.height / rows;

    // Grid states: 0 = unvisited, 1 = local (blue), 2 = remote (orange), 3 = both (green)
    const mapGrid = Array(rows).fill(0).map(() => Array(cols).fill(0));

    // Seed some initial visited rooms
    for (let r = 8; r < 16; r++) {
        for (let c = 10; c < 22; c++) {
            mapGrid[r][c] = (c < 18) ? 1 : 3;
        }
    }
    for (let r = 10; r < 18; r++) {
        for (let c = 22; c < 34; c++) {
            mapGrid[r][c] = 2;
        }
    }

    let localPos = { x: 14, y: 12 };
    let remotePos = { x: 30, y: 14 };
    let warpAnim = 0;

    function draw() {
        ctx.fillStyle = '#08070d';
        ctx.fillRect(0, 0, canvas.width, canvas.height);

        // Draw Tiles
        for (let r = 0; r < rows; r++) {
            for (let c = 0; c < cols; c++) {
                const state = mapGrid[r][c];
                if (state === 0) continue;

                if (state === 1) ctx.fillStyle = 'rgba(56, 189, 248, 0.75)'; // Blue (Local)
                else if (state === 2) ctx.fillStyle = 'rgba(249, 115, 22, 0.75)'; // Orange (Remote)
                else if (state === 3) ctx.fillStyle = 'rgba(34, 197, 94, 0.85)'; // Green (Both)

                ctx.fillRect(c * tileW, r * tileH, tileW - 1, tileH - 1);
            }
        }

        // Draw Local Player Dot (White / Cyan pulse)
        ctx.fillStyle = '#ffffff';
        ctx.beginPath();
        ctx.arc(localPos.x * tileW + tileW / 2, localPos.y * tileH + tileH / 2, 4.5, 0, Math.PI * 2);
        ctx.fill();

        ctx.strokeStyle = '#38bdf8';
        ctx.lineWidth = 2;
        ctx.beginPath();
        ctx.arc(localPos.x * tileW + tileW / 2, localPos.y * tileH + tileH / 2, 6.5, 0, Math.PI * 2);
        ctx.stroke();

        // Draw Remote Player Triangle (Red / Gold)
        const rx = remotePos.x * tileW + tileW / 2;
        const ry = remotePos.y * tileH + tileH / 2;

        ctx.fillStyle = '#ef4444';
        ctx.beginPath();
        ctx.moveTo(rx, ry - 6);
        ctx.lineTo(rx - 5, ry + 5);
        ctx.lineTo(rx + 5, ry + 5);
        ctx.closePath();
        ctx.fill();

        ctx.fillStyle = '#f5b942';
        ctx.font = '10px Outfit, sans-serif';
        ctx.fillText('Partner', rx + 8, ry + 2);

        // Warp Ripple Effect
        if (warpAnim > 0) {
            ctx.strokeStyle = `rgba(245, 185, 66, ${warpAnim / 20})`;
            ctx.lineWidth = 3;
            ctx.beginPath();
            ctx.arc(localPos.x * tileW + tileW / 2, localPos.y * tileH + tileH / 2, (20 - warpAnim) * 3, 0, Math.PI * 2);
            ctx.stroke();
            warpAnim--;
        }

        requestAnimationFrame(draw);
    }

    draw();

    // Button: Simulate Move
    const btnMove = document.getElementById('btnSimulateMove');
    if (btnMove) {
        btnMove.addEventListener('click', () => {
            const dx = Math.floor(Math.random() * 3) - 1;
            const dy = Math.floor(Math.random() * 3) - 1;
            remotePos.x = Math.max(2, Math.min(cols - 3, remotePos.x + dx));
            remotePos.y = Math.max(2, Math.min(rows - 3, remotePos.y + dy));

            // Mark tile
            const cur = mapGrid[remotePos.y][remotePos.x];
            if (cur === 1 || cur === 3) mapGrid[remotePos.y][remotePos.x] = 3;
            else mapGrid[remotePos.y][remotePos.x] = 2;

            trackEvent('radar_simulate_move', {
                category: 'Radar Simulator',
                player_x: remotePos.x,
                player_y: remotePos.y
            });
        });
    }

    // Button: Buddy Warp
    const btnWarp = document.getElementById('btnBuddyWarp');
    if (btnWarp) {
        btnWarp.addEventListener('click', () => {
            localPos.x = remotePos.x;
            localPos.y = remotePos.y;
            mapGrid[localPos.y][localPos.x] = 3;
            warpAnim = 20;

            trackEvent('radar_buddy_warp', {
                category: 'Radar Simulator',
                target_x: localPos.x,
                target_y: localPos.y
            });
        });
    }
}

// 2. Shortcuts Table Search
function initShortcutSearch() {
    const input = document.getElementById('shortcutSearch');
    const table = document.getElementById('shortcutsTable');
    if (!input || !table) return;

    let searchTimeout;

    input.addEventListener('input', () => {
        const query = input.value.toLowerCase().trim();
        const rows = table.querySelectorAll('tbody tr');
        let matchCount = 0;

        rows.forEach(row => {
            const text = row.innerText.toLowerCase();
            const matched = text.includes(query);
            row.style.display = matched ? '' : 'none';
            if (matched) matchCount++;
        });

        // Debounced search analytics
        clearTimeout(searchTimeout);
        if (query.length >= 2) {
            searchTimeout = setTimeout(() => {
                trackEvent('shortcut_search', {
                    category: 'Cheatsheet',
                    search_term: query,
                    results_count: matchCount
                });
            }, 600);
        }
    });
}

// 3. Navbar Scroll Blur
function initNavbarScroll() {
    const navbar = document.getElementById('navbar');
    if (!navbar) return;

    window.addEventListener('scroll', () => {
        if (window.scrollY > 50) {
            navbar.style.background = 'rgba(10, 9, 15, 0.95)';
            navbar.style.boxShadow = '0 4px 20px rgba(0, 0, 0, 0.6)';
        } else {
            navbar.style.background = 'rgba(18, 15, 29, 0.75)';
            navbar.style.boxShadow = 'none';
        }
    });
}

// 4. Copy Code Helper
window.copyCode = function(button) {
    const codeBox = button.closest('.code-box');
    const code = codeBox.querySelector('code').innerText;

    navigator.clipboard.writeText(code).then(() => {
        const icon = button.querySelector('i');
        icon.className = 'fa-solid fa-check text-success';
        setTimeout(() => {
            icon.className = 'fa-regular fa-copy';
        }, 2000);

        trackEvent('copy_code_snippet', {
            category: 'Developer',
            code_snippet: code.substring(0, 60)
        });
    });
};

// 5. Advanced GA4 Tracking (Outbound, CTAs, Sections, Scroll Depth, Heartbeat)
function initAnalyticsTrackers() {
    // A. Track All Outbound & CTA Clicks
    document.querySelectorAll('a').forEach(anchor => {
        anchor.addEventListener('click', (e) => {
            const href = anchor.getAttribute('href') || '';
            const text = (anchor.innerText || '').trim();

            // MiSTer 4 ALL Download Clicks
            if (href.includes('mister4all.com')) {
                trackEvent('mister4all_download_click', {
                    category: 'Conversion',
                    link_url: href,
                    button_text: text,
                    placement: anchor.closest('nav') ? 'navbar' : (anchor.closest('.mister-spotlight-card') ? 'spotlight' : (anchor.closest('.footer') ? 'footer' : 'body'))
                });
            }
            // YouTube Channel Clicks
            else if (href.includes('youtube.com')) {
                trackEvent('youtube_channel_click', {
                    category: 'Outbound',
                    link_url: href,
                    channel: '@GuhClemente'
                });
            }
            // GitHub Links
            else if (href.includes('github.com')) {
                trackEvent('github_link_click', {
                    category: 'Outbound',
                    link_url: href,
                    target: text
                });
            }
            // Internal Navigation Anchor Links
            else if (href.startsWith('#')) {
                trackEvent('navigation_click', {
                    category: 'Navigation',
                    target_section: href,
                    link_text: text
                });
            }
        });
    });

    // B. Track Section Views (Intersection Observer)
    if ('IntersectionObserver' in window) {
        const seenSections = new Set();
        const sectionObserver = new IntersectionObserver((entries) => {
            entries.forEach(entry => {
                if (entry.isIntersecting) {
                    const id = entry.target.id;
                    if (id && !seenSections.has(id)) {
                        seenSections.add(id);
                        trackEvent('section_view', {
                            category: 'Engagement',
                            section_id: id
                        });
                    }
                }
            });
        }, { threshold: 0.3 });

        document.querySelectorAll('section[id]').forEach(sec => {
            sectionObserver.observe(sec);
        });
    }

    // C. Scroll Depth Milestones (25%, 50%, 75%, 90%)
    const scrollMilestones = [25, 50, 75, 90];
    const reachedMilestones = new Set();

    window.addEventListener('scroll', () => {
        const scrollHeight = document.documentElement.scrollHeight - window.innerHeight;
        if (scrollHeight <= 0) return;
        const scrollPercent = Math.round((window.scrollY / scrollHeight) * 100);

        scrollMilestones.forEach(milestone => {
            if (scrollPercent >= milestone && !reachedMilestones.has(milestone)) {
                reachedMilestones.add(milestone);
                trackEvent('scroll_depth', {
                    category: 'Engagement',
                    percent_scrolled: milestone
                });
            }
        });
    });

    // D. Time on Page Heartbeat (30s, 60s, 120s, 300s)
    const timeMilestones = [30, 60, 120, 300];
    timeMilestones.forEach(seconds => {
        setTimeout(() => {
            trackEvent('time_on_page', {
                category: 'Engagement',
                seconds_spent: seconds
            });
        }, seconds * 1000);
    });
}

// 5. Interactive Character Roster & Spells Tabs
function initCharacterRoster() {
    const tabs = document.querySelectorAll('.roster-tab');
    const panes = document.querySelectorAll('.roster-pane');
    if (!tabs.length || !panes.length) return;

    tabs.forEach(tab => {
        tab.addEventListener('click', () => {
            const targetId = tab.getAttribute('data-target');
            if (!targetId) return;

            tabs.forEach(t => t.classList.remove('active'));
            panes.forEach(p => p.classList.remove('active'));

            tab.classList.add('active');
            const targetPane = document.getElementById(targetId);
            if (targetPane) {
                targetPane.classList.add('active');
            }

            trackEvent('select_character_roster', {
                category: 'Characters',
                character_id: targetId
            });
        });
    });
}
