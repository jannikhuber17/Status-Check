-- ============================================================
--  Staffel Viper Ready Check – Supabase Schema
--  Einzufügen in: Supabase Dashboard → SQL Editor → Run
-- ============================================================

-- 1. Tabelle anlegen
create table public.ready_check (
    uid          text        primary key,          -- TS3 Client Unique Identifier
    nickname     text        not null,
    status       integer     not null default 0,   -- 0=NotReady, 1=Standby, 2=Ready
    channel_name text        not null default 'viper_readycheck',
    updated_at   timestamptz not null default now()
);

-- 2. Row Level Security aktivieren (nötig damit anon key funktioniert)
alter table public.ready_check enable row level security;

create policy "anon_select" on public.ready_check
    for select using (true);

create policy "anon_insert" on public.ready_check
    for insert with check (true);

create policy "anon_update" on public.ready_check
    for update using (true);

create policy "anon_delete" on public.ready_check
    for delete using (true);

-- 3. Realtime für diese Tabelle aktivieren
--    (Postgres Changes → wird an alle WS-Subscriber gesendet)
alter publication supabase_realtime add table public.ready_check;

-- 4. updated_at automatisch aktualisieren
create or replace function public.touch_updated_at()
returns trigger language plpgsql as $$
begin
    new.updated_at = now();
    return new;
end;
$$;

create trigger ready_check_updated_at
    before update on public.ready_check
    for each row execute function public.touch_updated_at();

-- 5. Stale-Einträge aufräumen (optional)
--    Entfernt Einträge die seit mehr als 4 Stunden nicht aktualisiert wurden.
--    Kann manuell ausgeführt oder per pg_cron geplant werden.
--
-- delete from public.ready_check
--     where updated_at < now() - interval '4 hours';

-- ============================================================
--  Ergebnis:
--    Tabelle:   public.ready_check
--    Spalten:   uid (PK), nickname, status, channel_name, updated_at
--    Status:    0=Not Ready  1=Standby  2=Ready
--    Realtime:  Postgres Changes aktiv → Plugin empfängt INSERT/UPDATE/DELETE
-- ============================================================
