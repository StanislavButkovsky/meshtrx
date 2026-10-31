'use client';

import type React from 'react';

import { SITE } from '@/lib/constants';
import { useLanguage } from '@/components/LanguageProvider';

// Страница поддержки проекта. Оплата — через CloudTips (карты российских
// банков, 49–5 000 ₽ за раз). Для англоязычного сайта платёжной ссылки нет:
// иностранные карты CloudTips не принимает, поэтому там — только как помочь
// без денег.

type Item = { icon: React.ReactNode; title: string; text: string };

const stroke = { fill: 'none', stroke: 'currentColor', strokeWidth: 1.8, strokeLinecap: 'round' as const, strokeLinejoin: 'round' as const };

const Icon = {
  board: (
    <svg width="28" height="28" viewBox="0 0 24 24" {...stroke}>
      <rect x="4" y="7" width="16" height="12" rx="2" />
      <path d="M8 7V3M8 11h4M8 15h8" />
      <circle cx="17" cy="11" r="1" />
    </svg>
  ),
  field: (
    <svg width="28" height="28" viewBox="0 0 24 24" {...stroke}>
      <path d="M3 20l6-11 4 6 3-4 5 9z" />
      <path d="M16 4v4M14 6h4" />
    </svg>
  ),
  meter: (
    <svg width="28" height="28" viewBox="0 0 24 24" {...stroke}>
      <path d="M4 16a8 8 0 0 1 16 0" />
      <path d="M12 16l4-5" />
      <path d="M4 20h16" />
    </svg>
  ),
  server: (
    <svg width="28" height="28" viewBox="0 0 24 24" {...stroke}>
      <rect x="4" y="4" width="16" height="6" rx="2" />
      <rect x="4" y="14" width="16" height="6" rx="2" />
      <path d="M8 7h.01M8 17h.01" />
    </svg>
  ),
  chat: (
    <svg width="24" height="24" viewBox="0 0 24 24" {...stroke}>
      <path d="M4 5h16v11H9l-5 4z" />
    </svg>
  ),
  video: (
    <svg width="24" height="24" viewBox="0 0 24 24" {...stroke}>
      <rect x="3" y="6" width="13" height="12" rx="2" />
      <path d="M16 10l5-3v10l-5-3z" />
    </svg>
  ),
  people: (
    <svg width="24" height="24" viewBox="0 0 24 24" {...stroke}>
      <circle cx="9" cy="8" r="3" />
      <path d="M3 20a6 6 0 0 1 12 0" />
      <path d="M16 11a3 3 0 1 0 0-6M21 20a6 6 0 0 0-4-5.6" />
    </svg>
  ),
  code: (
    <svg width="24" height="24" viewBox="0 0 24 24" {...stroke}>
      <path d="M8 8l-4 4 4 4M16 8l4 4-4 4M13 6l-2 12" />
    </svg>
  ),
};

export default function DonatePage() {
  const { locale } = useLanguage();
  const ru = locale === 'ru';

  const spend: Item[] = ru
    ? [
        { icon: Icon.board, title: 'Платы и антенны', text: 'Каждую новую плату и ревизию — V3, обе V4, сборку на 433 МГц — нужно купить и проверить самому, прежде чем выпускать прошивку.' },
        { icon: Icon.field, title: 'Полевые испытания', text: 'Дальность, ретранслятор, связь через лес и дома проверяются только выездом: две рации, два человека, километры пешком.' },
        { icon: Icon.meter, title: 'Приборы для замеров', text: 'Чтобы советовать антенны и мощность по делу, а не на глаз, нужны анализатор антенн и измерения в эфире.' },
        { icon: Icon.server, title: 'Сервер и сайт', text: 'meshtrx.ru и meshtrx.com, загрузки прошивок и приложений, проверка обновлений для телефонов.' },
      ]
    : [];

  const help: Item[] = ru
    ? [
        { icon: Icon.chat, title: 'Пробуйте и рассказывайте', text: 'Нашли ошибку или чего-то не хватает — напишите в группу. Большая часть исправлений начинается с такого сообщения.' },
        { icon: Icon.video, title: 'Снимите ролик или напишите обзор', text: 'Короткое видео «как это звучит» или заметка о проверке в поле помогают людям решиться попробовать.' },
        { icon: Icon.people, title: 'Расскажите тем, у кого лежит плата', text: 'Многие купили Heltec под Meshtastic и забросили. Им достаточно одной прошивки, чтобы получить рацию.' },
        { icon: Icon.code, title: 'Код, переводы, вопросы', text: 'Исходники открыты на GitHub: предложения, исправления, переводы интерфейса и документации.' },
      ]
    : [
        { icon: Icon.chat, title: 'Try it and tell us', text: 'Found a bug or missing something — post in the group. Most fixes start with a message like that.' },
        { icon: Icon.video, title: 'Make a video or a write-up', text: 'A short "how it sounds" clip or a note from a field test helps others decide to try.' },
        { icon: Icon.people, title: 'Tell people with an idle board', text: 'Plenty of Heltec boards bought for Meshtastic sit in drawers. One flash turns them into a voice radio.' },
        { icon: Icon.code, title: 'Code, translations, questions', text: 'The source is open on GitHub: ideas, fixes, translations of the app and the docs.' },
      ];

  const helpLinks = ru
    ? [
        { href: 'https://t.me/MeshTRX', label: 'Группа в Telegram' },
        { href: SITE.github, label: 'GitHub' },
      ]
    : [
        { href: 'https://t.me/MeshTRX_en', label: 'Telegram group' },
        { href: SITE.github, label: 'GitHub' },
        { href: SITE.discussions, label: 'Discussions' },
      ];

  return (
    <div className="max-w-4xl mx-auto px-4 py-12">
      {/* Вступление */}
      <section className="relative overflow-hidden rounded-2xl border border-accent/40 bg-bg-card p-8 md:p-10 mb-10">
        <div className="absolute -right-16 -top-16 w-64 h-64 rounded-full bg-accent/10 blur-2xl pointer-events-none" />
        <div className="absolute right-8 top-8 hidden md:block text-accent/70">
          <svg width="96" height="96" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.2" strokeLinecap="round">
            <path d="M7 9a7 7 0 0 1 10 0M9.5 11.5a3.5 3.5 0 0 1 5 0" />
            <circle cx="12" cy="14" r="1" />
            <path d="M12 15v6M9 21h6" />
          </svg>
        </div>
        <p className="text-accent text-sm font-semibold tracking-wide uppercase mb-3">
          {ru ? 'Поддержать проект' : 'Support the project'}
        </p>
        <h1 className="text-3xl md:text-4xl font-bold text-text-primary mb-4 md:max-w-xl">
          {ru ? 'MeshTRX бесплатный — и останется таким' : 'MeshTRX is free — and will stay free'}
        </h1>
        <p className="text-text-secondary leading-relaxed md:max-w-2xl mb-6">
          {ru
            ? 'Прошивка, приложения для Android, iPhone и компьютера, сайт и документация делаются некоммерчески, для тех, кому нужна связь без интернета и сотовой сети. Если рация вам пригодилась, можно поддержать проект — добровольно, любой суммой.'
            : 'The firmware, the Android, iPhone and desktop apps, the site and the docs are a non-commercial project for people who need a radio that works without internet or cell service. If MeshTRX has been useful to you, there are a few ways to help.'}
        </p>
        {ru ? (
          <div className="flex flex-wrap items-center gap-4">
            <a
              href={SITE.donate}
              target="_blank"
              rel="noopener noreferrer"
              className="inline-flex items-center gap-2 px-6 py-3 rounded-xl bg-accent text-bg font-bold hover:bg-accent-dim transition-colors"
            >
              <svg width="20" height="20" viewBox="0 0 24 24" {...stroke}>
                <path d="M12 20s-7-4.4-7-10a4 4 0 0 1 7-2.6A4 4 0 0 1 19 10c0 5.6-7 10-7 10z" />
              </svg>
              Поддержать картой
            </a>
            <span className="text-sm text-text-secondary">через CloudTips · от 49 до 5 000 ₽ за раз</span>
          </div>
        ) : (
          <p className="text-sm text-text-secondary">
            Card payments currently work only with Russian bank cards, so below are the ways to help that work from anywhere.
          </p>
        )}
      </section>

      {/* На что идут деньги */}
      {ru && (
        <section className="mb-10">
          <h2 className="text-2xl font-bold text-text-primary mb-5">На что уходят деньги</h2>
          <div className="grid sm:grid-cols-2 gap-4">
            {spend.map((it) => (
              <div key={it.title} className="flex gap-4 p-5 rounded-xl bg-bg-card border border-border">
                <div className="w-12 h-12 flex-shrink-0 rounded-xl bg-accent/10 border border-accent/30 text-accent flex items-center justify-center">
                  {it.icon}
                </div>
                <div>
                  <h3 className="font-semibold text-text-primary mb-1">{it.title}</h3>
                  <p className="text-sm text-text-secondary leading-relaxed">{it.text}</p>
                </div>
              </div>
            ))}
          </div>
        </section>
      )}

      {/* Помочь без денег */}
      <section className="mb-10">
        <h2 className="text-2xl font-bold text-text-primary mb-2">
          {ru ? 'Помочь можно и без денег' : 'Ways to help'}
        </h2>
        <p className="text-text-secondary mb-5">
          {ru ? 'Проект растёт за счёт тех, кто им пользуется.' : 'The project grows through the people who use it.'}
        </p>
        <div className="grid sm:grid-cols-2 gap-4">
          {help.map((it) => (
            <div key={it.title} className="p-5 rounded-xl bg-bg-card border border-border">
              <div className="flex items-center gap-3 mb-2 text-secondary">
                {it.icon}
                <h3 className="font-semibold text-text-primary">{it.title}</h3>
              </div>
              <p className="text-sm text-text-secondary leading-relaxed">{it.text}</p>
            </div>
          ))}
        </div>
        <div className="flex flex-wrap gap-3 mt-5">
          {helpLinks.map((l) => (
            <a
              key={l.href}
              href={l.href}
              target="_blank"
              rel="noopener noreferrer"
              className="px-4 py-2 rounded-lg border border-accent/40 text-accent text-sm font-semibold hover:bg-accent/10 transition-colors"
            >
              {l.label}
            </a>
          ))}
        </div>
      </section>

      {/* Спасибо */}
      <section className="p-6 rounded-xl bg-bg-card border border-border text-center">
        <p className="text-text-primary font-semibold mb-1">
          {ru ? 'Спасибо всем, кто уже помогает' : 'Thank you to everyone who already helps'}
        </p>
        <p className="text-sm text-text-secondary">
          {ru
            ? 'Тестировщикам, авторам обзоров и всем, кто пишет в группу, — без вас половины исправлений не было бы.'
            : 'Testers, reviewers and everyone who posts in the group — half of the fixes would not exist without you.'}
        </p>
      </section>
    </div>
  );
}
