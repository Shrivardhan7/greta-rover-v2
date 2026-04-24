/*
 * Greta Rover OS
 * Copyright (c) 2026 Shrivardhan Jadhav
 * SPDX-License-Identifier: Apache-2.0
 * Licensed under Apache License 2.0
 */

'use strict';

const ROOM_STORAGE_KEYS = Object.freeze({
  HOME: 'greta_room_home',
  NODE: 'greta_room_node',
  PERSON: 'greta_room_person',
  DOOR: 'greta_room_door',
  SILENT: 'greta_room_silent',
  ACTIVITY: 'greta_room_activity',
});

const ROOM_LABELS = Object.freeze({
  owner_sister: 'Owner + Sister',
  master_bedroom: 'Master Bedroom',
  shared_bedroom: 'Shared Bedroom',
  living_area: 'Living Area',
  kitchen_dining: 'Kitchen + Dining',
  room: 'Room',
  entry: 'Entry',
  terrace: 'Terrace',
  balcony: 'Balcony',
  utility: 'Utility',
  toilet: 'Toilet',
  restricted: 'Restricted',
  parents: 'Parents',
  owner: 'Owner',
  sister: 'Sister',
  shared: 'Shared',
  restricted_owner: 'Restricted',
  unknown: 'Unknown',
});

const ROOM_HOME_PROFILES = Object.freeze({
  kolhapur: {
    label: 'Kolhapur (A-602)',
    nodes: [
      { value: 'bedroom_owner', label: 'Owner Bedroom' },
      { value: 'bedroom_sister', label: 'Sister Bedroom' },
      { value: 'bedroom_parents', label: 'Parents Bedroom' },
      { value: 'living_room', label: 'Living Room' },
      { value: 'kitchen_dining', label: 'Kitchen + Dining' },
      { value: 'toilets', label: 'Toilets' },
      { value: 'balcony', label: 'Balcony' },
      { value: 'utility', label: 'Utility' },
    ],
  },
  navi_mumbai: {
    label: 'Navi Mumbai (Flat 203)',
    nodes: [
      { value: 'N1', label: 'N1 Entrance' },
      { value: 'N2', label: 'N2 Living Room' },
      { value: 'N3', label: 'N3 Terrace' },
      { value: 'N4', label: 'N4 Kitchen' },
      { value: 'N5', label: 'N5 Master Bedroom' },
      { value: 'N6', label: 'N6 Shared Bedroom' },
      { value: 'N7', label: 'N7 Attached Toilet' },
      { value: 'N8', label: 'N8 Common Toilet' },
    ],
  },
});

const ROOM_TYPES = Object.freeze([
  { value: 'room', label: 'Room' },
  { value: 'master_bedroom', label: 'Master Bedroom' },
  { value: 'shared_bedroom', label: 'Shared Bedroom' },
  { value: 'living_area', label: 'Living Area' },
  { value: 'kitchen_dining', label: 'Kitchen + Dining' },
  { value: 'entry', label: 'Entry' },
  { value: 'terrace', label: 'Terrace' },
  { value: 'balcony', label: 'Balcony' },
  { value: 'utility', label: 'Utility' },
  { value: 'toilet', label: 'Toilet' },
  { value: 'restricted', label: 'Restricted' },
]);

const ROOM_OWNERS = Object.freeze([
  { value: 'unknown', label: 'Unknown' },
  { value: 'owner', label: 'Owner' },
  { value: 'sister', label: 'Sister' },
  { value: 'parents', label: 'Parents' },
  { value: 'owner_sister', label: 'Owner + Sister' },
  { value: 'shared', label: 'Shared' },
  { value: 'restricted', label: 'Restricted' },
]);

const ROOM_PEOPLE = Object.freeze([
  { value: 'unknown', label: 'Unknown' },
  { value: 'owner', label: 'Owner' },
  { value: 'sister', label: 'Sister' },
  { value: 'parents', label: 'Parents' },
  { value: 'owner_sister', label: 'Owner + Sister' },
  { value: 'cat', label: 'Cat' },
]);

const roomState = {
  home: 'kolhapur',
  node: '',
  person: 'unknown',
  doorClosed: false,
  silent: false,
  activity: false,
};

const roomDom = {};

function roomStorageGet(key) {
  try {
    return localStorage.getItem(key);
  } catch (_) {
    return null;
  }
}

function roomStorageSet(key, value) {
  try {
    localStorage.setItem(key, value);
  } catch (_) {
    // Ignore storage failures in restricted contexts.
  }
}

function roomBoolFromStorage(key) {
  return roomStorageGet(key) === '1';
}

function roomResolveDom() {
  const ids = [
    'roomHomeValue', 'roomNodeValue', 'roomTypeValue', 'roomOwnerValue',
    'roomConfidenceValue', 'roomReviewValue', 'roomSignalsValue',
    'roomAlternativeValue', 'roomVerifiedValue',
    'roomHomeSelect', 'roomNodeSelect', 'roomPersonSelect',
    'roomDoorClosed', 'roomSilent', 'roomActivity',
    'roomObserveBtn', 'roomOverrideNodeSelect', 'roomOverrideTypeSelect',
    'roomOverrideOwnerSelect', 'roomOverrideBtn', 'roomClearOverrideBtn',
    'connectBtn',
  ];

  ids.forEach(id => {
    const element = document.getElementById(id);
    if (element) {
      roomDom[id] = element;
      if (!element.dataset.baseClass) {
        element.dataset.baseClass = element.className || '';
      }
    }
  });
}

function roomFormatToken(token) {
  if (!token) return '-';
  if (ROOM_LABELS[token]) return ROOM_LABELS[token];
  return String(token)
    .replace(/_/g, ' ')
    .replace(/\b\w/g, character => character.toUpperCase());
}

function roomSetValue(element, text, tone) {
  if (!element) return;
  element.textContent = text;
  const baseClass = element.dataset.baseClass || '';
  element.className = `${baseClass} ${tone || ''}`.trim();
}

function roomPopulateSelect(select, options, selectedValue) {
  if (!select) return;

  const fragment = document.createDocumentFragment();
  options.forEach(option => {
    const optionElement = document.createElement('option');
    optionElement.value = option.value;
    optionElement.textContent = option.label;
    if (option.value === selectedValue) {
      optionElement.selected = true;
    }
    fragment.appendChild(optionElement);
  });

  select.innerHTML = '';
  select.appendChild(fragment);
}

function roomActiveProfile() {
  return ROOM_HOME_PROFILES[roomState.home] || ROOM_HOME_PROFILES.kolhapur;
}

function roomProfileNodeOptions() {
  return roomActiveProfile().nodes;
}

function roomDefaultNode(home) {
  const profile = ROOM_HOME_PROFILES[home] || ROOM_HOME_PROFILES.kolhapur;
  return profile.nodes.length > 0 ? profile.nodes[0].value : '';
}

function roomApplyStoredState() {
  const storedHome = roomStorageGet(ROOM_STORAGE_KEYS.HOME);
  if (storedHome && ROOM_HOME_PROFILES[storedHome]) {
    roomState.home = storedHome;
  }

  const storedPerson = roomStorageGet(ROOM_STORAGE_KEYS.PERSON);
  if (storedPerson) {
    roomState.person = storedPerson;
  }

  roomState.doorClosed = roomBoolFromStorage(ROOM_STORAGE_KEYS.DOOR);
  roomState.silent = roomBoolFromStorage(ROOM_STORAGE_KEYS.SILENT);
  roomState.activity = roomBoolFromStorage(ROOM_STORAGE_KEYS.ACTIVITY);
  roomState.node = roomStorageGet(ROOM_STORAGE_KEYS.NODE) || roomDefaultNode(roomState.home);
}

function roomPersistState() {
  roomStorageSet(ROOM_STORAGE_KEYS.HOME, roomState.home);
  roomStorageSet(ROOM_STORAGE_KEYS.NODE, roomState.node);
  roomStorageSet(ROOM_STORAGE_KEYS.PERSON, roomState.person);
  roomStorageSet(ROOM_STORAGE_KEYS.DOOR, roomState.doorClosed ? '1' : '0');
  roomStorageSet(ROOM_STORAGE_KEYS.SILENT, roomState.silent ? '1' : '0');
  roomStorageSet(ROOM_STORAGE_KEYS.ACTIVITY, roomState.activity ? '1' : '0');
}

function roomRefreshSelects() {
  roomPopulateSelect(
    roomDom.roomHomeSelect,
    Object.entries(ROOM_HOME_PROFILES).map(([value, profile]) => ({ value, label: profile.label })),
    roomState.home
  );

  const nodeOptions = roomProfileNodeOptions();
  if (!nodeOptions.some(option => option.value === roomState.node)) {
    roomState.node = roomDefaultNode(roomState.home);
  }

  roomPopulateSelect(roomDom.roomNodeSelect, nodeOptions, roomState.node);
  roomPopulateSelect(roomDom.roomOverrideNodeSelect, nodeOptions, roomState.node);
  roomPopulateSelect(roomDom.roomPersonSelect, ROOM_PEOPLE, roomState.person);
  roomPopulateSelect(roomDom.roomOverrideTypeSelect, ROOM_TYPES, 'room');
  roomPopulateSelect(roomDom.roomOverrideOwnerSelect, ROOM_OWNERS, 'unknown');

  if (roomDom.roomDoorClosed) roomDom.roomDoorClosed.checked = roomState.doorClosed;
  if (roomDom.roomSilent) roomDom.roomSilent.checked = roomState.silent;
  if (roomDom.roomActivity) roomDom.roomActivity.checked = roomState.activity;
}

function roomBuildObserveCommand() {
  const now = new Date();
  const hour = now.getHours();
  const iso = now.toISOString();
  const personConfidence = roomState.person === 'unknown' ? 0 : 95;
  const door = roomState.doorClosed ? 1 : 0;
  const silent = roomState.silent ? 1 : 0;
  const activity = roomState.activity ? 1 : 0;
  return `ROOM OBSERVE ${roomState.node} ${roomState.person} ${personConfidence} ${hour} ${door} ${silent} ${activity} ${iso}`;
}

function roomSendCommand(command, logMessage, logClass, silentFailure) {
  if (typeof ws_send !== 'function' || !ws_send(command)) {
    if (!silentFailure && typeof log_add === 'function') {
      log_add('Not connected - room command dropped', 'ev-error');
    }
    return false;
  }

  if (logMessage && typeof log_add === 'function') {
    log_add(logMessage, logClass || 'ev-room');
  }
  return true;
}

function roomSendHome(silent) {
  return roomSendCommand(
    `ROOM HOME ${roomState.home}`,
    silent ? '' : `Room home -> ${roomFormatToken(roomState.home)}`,
    'ev-room',
    silent
  );
}

function roomSendSnapshot(silent) {
  roomPersistState();
  if (!roomState.node) return false;
  if (!roomSendHome(true)) return false;
  return roomSendCommand(
    roomBuildObserveCommand(),
    silent ? '' : `Room snapshot -> ${roomFormatToken(roomState.node)}`,
    'ev-room',
    silent
  );
}

function roomSendOverride() {
  const node = roomDom.roomOverrideNodeSelect ? roomDom.roomOverrideNodeSelect.value : roomState.node;
  const roomType = roomDom.roomOverrideTypeSelect ? roomDom.roomOverrideTypeSelect.value : 'room';
  const owner = roomDom.roomOverrideOwnerSelect ? roomDom.roomOverrideOwnerSelect.value : 'unknown';

  if (!node) return false;
  if (!roomSendHome(true)) return false;

  return roomSendCommand(
    `ROOM SET ${node} ${roomType} ${owner}`,
    `Room override saved for ${roomFormatToken(node)}`,
    'ev-room',
    false
  );
}

function roomClearOverride() {
  const node = roomDom.roomOverrideNodeSelect ? roomDom.roomOverrideNodeSelect.value : roomState.node;
  if (!node) return false;
  if (!roomSendHome(true)) return false;

  return roomSendCommand(
    `ROOM CLEAR ${node}`,
    `Room override cleared for ${roomFormatToken(node)}`,
    'ev-room',
    false
  );
}

function roomUpdateStatus(data) {
  if (!data) return;

  if (data.home && ROOM_HOME_PROFILES[data.home] && data.home !== roomState.home) {
    roomState.home = data.home;
    roomRefreshSelects();
    roomPersistState();
  }

  roomSetValue(roomDom.roomHomeValue, roomFormatToken(data.home || roomState.home), 'accent');
  roomSetValue(roomDom.roomNodeValue, roomFormatToken(data.roomNode), '');
  roomSetValue(roomDom.roomTypeValue, roomFormatToken(data.roomType), data.roomRestricted ? 'warn' : '');
  roomSetValue(roomDom.roomOwnerValue, roomFormatToken(data.roomOwner), '');

  const confidence = Number(data.roomConfidence);
  const confidenceText = Number.isFinite(confidence) && confidence > 0
    ? `${Math.round(confidence * 100)}%`
    : '-';
  const confidenceTone = confidence >= 0.8 ? 'ok' : confidence >= 0.6 ? 'warn' : 'error';
  roomSetValue(roomDom.roomConfidenceValue, confidenceText, confidenceText === '-' ? '' : confidenceTone);

  const needsReview = Boolean(data.roomNeedsReview);
  roomSetValue(roomDom.roomReviewValue, needsReview ? 'REVIEW REQUIRED' : 'STABLE', needsReview ? 'warn' : 'ok');
  roomSetValue(
    roomDom.roomSignalsValue,
    data.roomSignals ? String(data.roomSignals).split(',').map(roomFormatToken).join(', ') : '-',
    ''
  );

  const altConfidence = Number(data.roomAltConfidence);
  const altText = data.roomAltType
    ? `${roomFormatToken(data.roomAltType)} (${Math.round(altConfidence * 100)}%)`
    : '-';
  roomSetValue(roomDom.roomAlternativeValue, altText, '');
  roomSetValue(roomDom.roomVerifiedValue, data.roomLastVerified || '-', '');
}

function roomHandleAck(data) {
  if (!data) return;
  const action = data.action ? String(data.action).toUpperCase() : 'ROOM';

  if (typeof log_add === 'function' && action !== 'HOME' && action !== 'OBSERVE') {
    const reason = data.reason ? ` (${data.reason})` : '';
    log_add(`Room ${action} acknowledged${reason}`, 'ev-room');
  }

  if (data.home && ROOM_HOME_PROFILES[data.home]) {
    roomState.home = data.home;
    roomRefreshSelects();
    roomPersistState();
  }
}

function roomHandleRejected(data) {
  if (typeof log_add === 'function') {
    const action = data && data.action ? String(data.action).toUpperCase() : 'ROOM';
    const reason = data && data.reason ? data.reason : 'rejected';
    log_add(`Room ${action} rejected - ${reason}`, 'ev-error');
  }
}

function roomQueueSync(delayMs) {
  window.clearTimeout(roomQueueSync.timerId);
  roomQueueSync.timerId = window.setTimeout(() => {
    roomSendSnapshot(true);
  }, delayMs);
}

function roomBindEvents() {
  if (roomDom.roomHomeSelect) {
    roomDom.roomHomeSelect.addEventListener('change', event => {
      roomState.home = event.target.value;
      roomState.node = roomDefaultNode(roomState.home);
      roomRefreshSelects();
      roomPersistState();
      roomSendHome(false);
      roomQueueSync(300);
    });
  }

  if (roomDom.roomNodeSelect) {
    roomDom.roomNodeSelect.addEventListener('change', event => {
      roomState.node = event.target.value;
      roomPersistState();
    });
  }

  if (roomDom.roomPersonSelect) {
    roomDom.roomPersonSelect.addEventListener('change', event => {
      roomState.person = event.target.value;
      roomPersistState();
    });
  }

  if (roomDom.roomDoorClosed) {
    roomDom.roomDoorClosed.addEventListener('change', event => {
      roomState.doorClosed = event.target.checked;
      roomPersistState();
    });
  }

  if (roomDom.roomSilent) {
    roomDom.roomSilent.addEventListener('change', event => {
      roomState.silent = event.target.checked;
      roomPersistState();
    });
  }

  if (roomDom.roomActivity) {
    roomDom.roomActivity.addEventListener('change', event => {
      roomState.activity = event.target.checked;
      roomPersistState();
    });
  }

  if (roomDom.roomObserveBtn) {
    roomDom.roomObserveBtn.addEventListener('click', () => {
      roomSendSnapshot(false);
    });
  }

  if (roomDom.roomOverrideBtn) {
    roomDom.roomOverrideBtn.addEventListener('click', () => {
      roomSendOverride();
    });
  }

  if (roomDom.roomClearOverrideBtn) {
    roomDom.roomClearOverrideBtn.addEventListener('click', () => {
      roomClearOverride();
    });
  }

  if (roomDom.connectBtn) {
    roomDom.connectBtn.addEventListener('click', () => {
      roomQueueSync(1200);
    });
  }
}

function roomPatchWsOnMessage() {
  const original = window.ws_on_message;
  if (typeof original !== 'function') return;

  window.ws_on_message = function patchedRoomWsOnMessage(raw) {
    if (typeof raw === 'string' && raw.trim().startsWith('{')) {
      try {
        const data = JSON.parse(raw);

        if (data.event === 'ROOM_ACK') {
          roomHandleAck(data);
          return;
        }

        if (data.event === 'ROOM_REJECTED') {
          roomHandleRejected(data);
          return;
        }

        if (Object.prototype.hasOwnProperty.call(data, 'roomType')
            || Object.prototype.hasOwnProperty.call(data, 'roomNode')) {
          roomUpdateStatus(data);
        }
      } catch (_) {
        // Fall through to the original message handler.
      }
    }

    original(raw);
  };
}

function roomInit() {
  roomResolveDom();
  roomApplyStoredState();
  roomRefreshSelects();
  roomBindEvents();
  roomPatchWsOnMessage();

  roomSetValue(roomDom.roomHomeValue, roomFormatToken(roomState.home), 'accent');
  roomSetValue(roomDom.roomNodeValue, roomFormatToken(roomState.node), '');
  roomSetValue(roomDom.roomTypeValue, '-', '');
  roomSetValue(roomDom.roomOwnerValue, '-', '');
  roomSetValue(roomDom.roomConfidenceValue, '-', '');
  roomSetValue(roomDom.roomReviewValue, 'AWAITING SNAPSHOT', 'warn');
  roomSetValue(roomDom.roomSignalsValue, '-', '');
  roomSetValue(roomDom.roomAlternativeValue, '-', '');
  roomSetValue(roomDom.roomVerifiedValue, '-', '');

  window.setInterval(() => {
    roomSendSnapshot(true);
  }, 60000);

  roomQueueSync(1500);
}

if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', roomInit);
} else {
  roomInit();
}
