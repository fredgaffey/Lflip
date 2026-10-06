<template>
  <ion-page>
    <ion-header>
      <ion-toolbar>
        <ion-buttons slot="start" v-if="activeChat">
          <ion-button @click="closeChat" aria-label="Back to chats">
            <ion-icon :icon="arrowBack" slot="icon-only"></ion-icon>
          </ion-button>
        </ion-buttons>
        <ion-title>{{ activeChat ? (activeChat.chat_name || 'AI Chat') : 'Assistant' }}</ion-title>
      </ion-toolbar>
    </ion-header>

    <ion-content ref="contentRef" :scroll-y="true">
      <!-- chat list -->
      <template v-if="!activeChat">
        <div v-if="chatsStore.chats.value.length === 0" class="empty-state">
          <ion-icon :icon="chatbubblesOutline" class="empty-icon"></ion-icon>
          <h2>Your driving assistant</h2>
          <p>Ask about road rules, your logged trips, or licence requirements.</p>
          <p class="tools-hint">Choose what data I can access in <strong>Settings</strong>.</p>
          <ion-button @click="newChat">
            <ion-icon :icon="add" slot="start"></ion-icon>
            Start a chat
          </ion-button>
        </div>

        <ion-list v-else lines="none">
          <ion-item-sliding v-for="chat in chatsStore.chats.value" :key="chat.id">
            <ion-item button detail @click="openChat(chat)" class="chat-row">
              <div class="chat-avatar" slot="start">
                <ion-icon :icon="chatbubblesOutline"></ion-icon>
              </div>
              <ion-label>
                <h2>{{ chat.chat_name || 'Untitled chat' }}</h2>
                <p v-if="chat.created_at">{{ formatDate(chat.created_at) }}</p>
              </ion-label>
            </ion-item>
            <ion-item-options side="end">
              <ion-item-option color="danger" @click="removeChat(chat.id)">
                <ion-icon :icon="trashOutline" slot="icon-only"></ion-icon>
              </ion-item-option>
            </ion-item-options>
          </ion-item-sliding>
        </ion-list>

        <ion-fab slot="fixed" vertical="bottom" horizontal="end">
          <ion-fab-button @click="newChat" aria-label="New chat">
            <ion-icon :icon="add"></ion-icon>
          </ion-fab-button>
        </ion-fab>
      </template>

      <!-- messages -->
      <div v-else class="messages">
        <div v-if="chatsStore.messages.value.length === 0 && !sending" class="thread-hint">
          <ion-icon :icon="chatbubblesOutline" class="empty-icon"></ion-icon>
          <p>Ask me anything about learning to drive.</p>
          <p class="tools-hint">You can choose what data I'm allowed to access in <strong>Settings</strong>.</p>
        </div>

        <div v-for="m in chatsStore.messages.value" :key="m.id"
             class="row" :class="m.is_ai ? 'row-ai' : 'row-user'">
          <div v-if="m.is_ai" class="msg-avatar">
            <span class="lplate">L</span>
          </div>
          <div class="bubble" :class="m.is_ai ? 'bubble-ai' : 'bubble-user'">
            <div v-if="m.is_ai" class="md" v-html="render(m.content)"></div>
            <template v-else>{{ m.content }}</template>
          </div>
        </div>

        <div v-if="sending" class="row row-ai">
          <div class="msg-avatar">
            <span class="lplate">L</span>
          </div>
          <div class="bubble bubble-ai typing">
            <span></span><span></span><span></span>
          </div>
        </div>
      </div>
    </ion-content>

    <ion-footer v-if="activeChat">
      <div class="composer-bar">
        <ion-button
          shape="round"
          fill="clear"
          color="medium"
          class="chats-btn"
          @click="closeChat"
          aria-label="Back to chats"
        >
          <ion-icon :icon="listOutline" slot="icon-only"></ion-icon>
        </ion-button>
        <ion-textarea
          ref="inputRef"
          v-model="prompt"
          placeholder="Ask about road rules, your trips, or licence info…"
          :auto-grow="true"
          :rows="1"
          :disabled="sending"
          class="composer-input"
          @keydown="onKeydown"
        ></ion-textarea>
        <ion-button
          shape="round"
          fill="solid"
          color="primary"
          class="send-btn"
          :disabled="sending || !prompt.trim()"
          @click="send"
          aria-label="Send"
        >
          <ion-icon :icon="send_icon" slot="icon-only"></ion-icon>
        </ion-button>
      </div>
    </ion-footer>
  </ion-page>
</template>

<script setup lang="ts">
import {
  IonPage, IonContent, IonHeader, IonToolbar, IonTitle, IonFooter,
  IonButton, IonButtons, IonIcon, IonList, IonItem, IonLabel,
  IonTextarea, IonItemSliding, IonItemOptions, IonItemOption,
  IonFab, IonFabButton,
} from '@ionic/vue'
import {
  add, arrowBack, send as send_icon,
  chatbubblesOutline, trashOutline, listOutline,
} from 'ionicons/icons'
import { ref, onMounted, nextTick } from 'vue'
import { marked } from 'marked'
import DOMPurify from 'dompurify'
import { chatsStore, type Chat } from './classes/chats'

const activeChat = ref<Chat | null>(null)
const prompt = ref('')
const sending = ref(false)
const contentRef = ref<InstanceType<typeof IonContent> | null>(null)
const inputRef = ref<InstanceType<typeof IonTextarea> | null>(null)

marked.setOptions({ breaks: true, gfm: true })

function render(text: string): string {
  // the model emits narrow/zero-width spaces that some fonts render as nothing
  const clean = (text ?? '').replace(/[  -​  　]/g, ' ')
  const html = marked.parse(clean) as string
  // wrap tables so a wide one (e.g. all 8 states) scrolls sideways inside the
  // bubble instead of squashing every column until the text wraps letter by letter
  const wrapped = html.replace(/<table>/g, '<div class="table-scroll"><table>')
                      .replace(/<\/table>/g, '</table></div>')
  return DOMPurify.sanitize(wrapped)
}

function formatDate(ts: number): string {
  const d = new Date(ts < 1e12 ? ts * 1000 : ts)
  return d.toLocaleDateString(undefined, { day: 'numeric', month: 'short', year: 'numeric' })
}

async function scrollToBottom(smooth = true) {
  await nextTick()
  await contentRef.value?.$el?.scrollToBottom(smooth ? 300 : 0)
}

onMounted(async () => {
  await chatsStore.pull_chats()
})

async function focusInput() {
  await nextTick()
  // setFocus brings up the on-screen keyboard on mobile, unlike autofocus
  await inputRef.value?.$el?.setFocus()
}

async function openChat(chat: Chat) {
  activeChat.value = chat
  // focus before the network fetch: iOS only raises the keyboard while still
  // within the tap gesture, and awaiting pull_messages first loses that window
  await focusInput()
  await chatsStore.pull_messages(chat.id)
  await scrollToBottom(false)
}

function closeChat() {
  activeChat.value = null
}

async function newChat() {
  const chat = await chatsStore.create_chat()
  if (!chat) {
    alert('Could not start a new chat. Check your connection and try again.')
    return
  }
  await openChat(chat)
}

async function removeChat(id: number) {
  await chatsStore.delete_chat(id)
}

function onKeydown(e: KeyboardEvent) {
  if (e.key === 'Enter' && !e.shiftKey) {
    e.preventDefault()
    send()
  }
}

async function send() {
  if (!activeChat.value || !prompt.value.trim()) return
  const text = prompt.value.trim()
  const wasFirstMessage = chatsStore.messages.value.length === 0
  prompt.value = ''
  sending.value = true
  await scrollToBottom()
  try {
    const res = await chatsStore.send_message(activeChat.value.id, text)
    if (res.status === 429) {
      // 429 is a hit limit, not a connection problem - say which so the user
      // isn't told to "check your connection" when nothing is wrong with it
      const msg = String(res.data?.message ?? '')
      alert(/token|limit/i.test(msg)
        ? "You've reached the AI assistant's usage limit for now. It refreshes after a few hours."
        : "You're sending messages too quickly. Wait a moment and try again.")
    } else if (res.status !== 200) {
      alert('Message failed to send. Check your connection and try again.')
    } else if (wasFirstMessage) {
      // the AI auto-names the chat after the first message; refresh so the
      // header/list show the new name instead of "Untitled chat"
      await chatsStore.pull_chats()
      const updated = chatsStore.chats.value.find(c => c.id === activeChat.value?.id)
      if (updated) activeChat.value = updated
    }
    await scrollToBottom()
  } finally {
    sending.value = false
  }
  await focusInput()
}
</script>

<style scoped>
/* ===== empty states ===== */
.empty-state {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  text-align: center;
  min-height: 70vh;
  padding: 24px;
  gap: 6px;
}
.empty-icon {
  font-size: 56px;
  color: var(--ion-color-medium);
  margin-bottom: 8px;
}
.empty-state h2 {
  margin: 0;
  font-weight: 700;
}
.empty-state p {
  margin: 0 0 16px;
  color: var(--ion-color-medium-shade);
  max-width: 280px;
}

/* ===== chat list ===== */
.chat-avatar {
  width: 40px;
  height: 40px;
  border-radius: 50%;
  background: var(--ion-color-primary);
  color: var(--ion-color-primary-contrast);
  display: flex;
  align-items: center;
  justify-content: center;
  margin-right: 12px;
  font-size: 20px;
}
.chat-row ion-label h2 {
  font-weight: 600;
}

/* ===== messages thread ===== */
.messages {
  display: flex;
  flex-direction: column;
  gap: 10px;
  padding: 16px 12px;
}
.thread-hint {
  text-align: center;
  color: var(--ion-color-medium-shade);
  margin: 32px 0;
}
.tools-hint {
  font-size: 0.85em;
  color: var(--ion-color-medium);
}
.row {
  display: flex;
  align-items: flex-end;
  gap: 8px;
  max-width: 100%;
}
.row-user {
  justify-content: flex-end;
}
.row-ai {
  justify-content: flex-start;
}
.msg-avatar {
  flex: 0 0 28px;
  width: 28px;
  height: 28px;
  display: flex;
  align-items: center;
  justify-content: center;
}
/* mini L-plate: white tile, red "L", red border */
.lplate {
  width: 26px;
  height: 26px;
  border-radius: 6px;
  background: var(--lp-yellow, #c5bf10);
  color: var(--lp-dark, #1d1d1d);
  font-weight: 800;
  font-size: 16px;
  line-height: 1;
  display: flex;
  align-items: center;
  justify-content: center;
}
.bubble {
  max-width: 78%;
  padding: 10px 14px;
  border-radius: 18px;
  white-space: pre-wrap;
  line-height: 1.4;
  box-shadow: 0 1px 2px rgba(0, 0, 0, 0.08);
}
.bubble-user {
  background: var(--ion-color-primary);
  color: var(--ion-color-primary-contrast);
  border-bottom-right-radius: 4px;
}
.bubble-ai {
  max-width: 92%;
  background: var(--ion-background-color-step-100, #f2f2f2);
  color: var(--ion-text-color);
  border-bottom-left-radius: 4px;
}

/* markdown inside AI bubbles */
.md {
  white-space: normal;
}
.md :first-child { margin-top: 0; }
.md :last-child { margin-bottom: 0; }
.md p { margin: 0 0 8px; }
.md ul, .md ol { margin: 4px 0 8px; padding-left: 20px; }
.md li { margin: 2px 0; }
.md h1, .md h2, .md h3 { margin: 8px 0 4px; font-size: 1.05em; }
.md code {
  background: var(--ion-color-light-shade);
  padding: 1px 5px;
  border-radius: 5px;
  font-size: 0.9em;
}
.md pre {
  background: var(--lp-dark, #1d1d1d);
  color: #f4f4f4;
  padding: 10px 12px;
  border-radius: 10px;
  overflow-x: auto;
}
.md pre code { background: none; padding: 0; }
.md a { color: var(--ion-color-primary); }

/* tables: the wrapper scrolls sideways so a wide table keeps its columns
   instead of squashing each one until the text wraps letter by letter */
.md .table-scroll {
  overflow-x: auto;
  -webkit-overflow-scrolling: touch;
  margin: 8px 0;
  max-width: 100%;
}
.md table {
  border-collapse: collapse;
  font-size: 0.88em;
}
.md th,
.md td {
  border: 1px solid var(--ion-color-step-200, rgba(128, 128, 128, 0.3));
  padding: 5px 10px;
  text-align: left;
  white-space: nowrap;
}
.md th {
  background: var(--ion-color-light-shade);
  font-weight: 600;
}
.md blockquote {
  margin: 8px 0;
  padding: 2px 12px;
  border-left: 3px solid var(--ion-color-medium);
  color: var(--ion-color-medium-shade);
}

/* typing indicator */
.typing {
  display: flex;
  gap: 4px;
  align-items: center;
}
.typing span {
  width: 7px;
  height: 7px;
  border-radius: 50%;
  background: var(--ion-color-medium);
  animation: blink 1.4s infinite both;
}
.typing span:nth-child(2) { animation-delay: 0.2s; }
.typing span:nth-child(3) { animation-delay: 0.4s; }
@keyframes blink {
  0%, 80%, 100% { opacity: 0.3; }
  40% { opacity: 1; }
}

/* ===== composer ===== */
.composer-bar {
  display: flex;
  align-items: flex-end;
  gap: 8px;
  padding: 8px;
  background: var(--ion-toolbar-background);
}
.composer-input {
  flex: 1 1 auto;
  min-width: 0;
  width: 100%;
  /* 16px stops iOS zooming into the field on focus, which was throwing off its
     size on phones */
  font-size: 16px;
  --background: var(--ion-color-light);
  --color: var(--ion-color-light-contrast);
  --placeholder-color: var(--ion-color-medium);
  --placeholder-opacity: 1;
  --padding-start: 14px;
  --padding-end: 14px;
  --padding-top: 10px;
  --padding-bottom: 10px;
  --border-radius: 20px;
  border-radius: 20px;
  min-height: 40px;
  max-height: 120px;
}
.send-btn,
.chats-btn {
  flex: 0 0 auto;
  --padding-start: 0;
  --padding-end: 0;
  width: 44px;
  height: 44px;
  margin: 0;
}
</style>
