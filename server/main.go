package main

import (
	"crypto/rand"
	"encoding/binary"
	"errors"
	"fmt"
	"io"
	"log"
	"net"
	"os"
	"strconv"
	"strings"
	"sync"
)

const (
	packetHello   = 1
	packetWelcome = 2
	packetLobby   = 3
	packetProfile = 4
	packetStart   = 5
	packetKick    = 6
	packetInfo    = 7
	packetInput   = 8
	packetFrame   = 9
	packetSave    = 10
	packetEnd     = 11
	packetError   = 12
	packetPause   = 13
)

type peer struct {
	conn      net.Conn
	room      *room
	isHost    bool
	name      string
	skin      int
	control   int
	closeOnce sync.Once
	writeMu   sync.Mutex
}

type room struct {
	code        string
	host        *peer
	client      *peer
	gameStarted bool
	hostName    string
	hostSkin    int
	hostControl int
	clientName  string
	clientSkin  int
	clientControl int
	mu          sync.Mutex
}

var (
	roomsMu sync.Mutex
	rooms   = map[string]*room{}
)

func main() {
	addr := ":" + envDefault("PORT", "9090")
	ln, err := net.Listen("tcp", addr)
	if err != nil {
		log.Fatalf("listen %s: %v", addr, err)
	}
	log.Printf("online relay listening on %s", addr)

	for {
		conn, err := ln.Accept()
		if err != nil {
			log.Printf("accept: %v", err)
			continue
		}
		go handleConn(conn)
	}
}

func handleConn(conn net.Conn) {
	defer conn.Close()

	if tcpConn, ok := conn.(*net.TCPConn); ok {
		_ = tcpConn.SetNoDelay(true)
	}

	p := &peer{
		conn:    conn,
		name:    "PLAYER",
		skin:    1,
		control: 2,
	}

	packetType, payload, err := readPacket(conn)
	if err != nil {
		log.Printf("read hello: %v", err)
		return
	}
	if packetType != packetHello {
		writePacket(conn, packetError, []byte("message=EXPECTED_HELLO"))
		return
	}

	if err := attachPeerFromHello(p, string(payload)); err != nil {
		writePacket(conn, packetError, []byte("message="+sanitizeToken(err.Error(), true)))
		return
	}

	writePacket(conn, packetWelcome, []byte("code="+p.room.code))
	broadcastLobby(p.room, "CONNECTED")

	for {
		packetType, payload, err = readPacket(conn)
		if err != nil {
			if !errors.Is(err, io.EOF) {
				log.Printf("peer %s read error: %v", p.name, err)
			}
			cleanupPeer(p)
			return
		}

		switch packetType {
		case packetProfile:
			handleProfilePacket(p, string(payload))
		case packetStart:
			handleStartPacket(p)
		case packetKick:
			handleKickPacket(p)
		case packetInput:
			handleInputPacket(p, payload)
		case packetFrame:
			handleFramePacket(p, payload)
		case packetSave:
			handleSavePacket(p, payload)
		case packetEnd:
			handleEndPacket(p, payload)
		case packetPause:
			handlePausePacket(p, payload)
		}
	}
}

func attachPeerFromHello(p *peer, payload string) error {
	role := valueFor(payload, "role")
	name := sanitizeToken(valueFor(payload, "name"), false)
	if name == "" {
		name = "PLAYER"
	}

	switch role {
	case "host":
		r := createRoom()
		r.mu.Lock()
		r.host = p
		r.hostName = name
		r.hostSkin = 1
		r.hostControl = 2
		r.mu.Unlock()

		p.room = r
		p.isHost = true
		p.name = name
		return nil

	case "join":
		code := sanitizeToken(valueFor(payload, "code"), true)
		if code == "" {
			return fmt.Errorf("invalid_join_code")
		}

		roomsMu.Lock()
		r, ok := rooms[code]
		roomsMu.Unlock()
		if !ok {
			return fmt.Errorf("room_not_found")
		}

		r.mu.Lock()
		defer r.mu.Unlock()
		if r.client != nil {
			return fmt.Errorf("room_full")
		}
		if r.gameStarted {
			return fmt.Errorf("game_in_progress")
		}

		r.client = p
		if r.clientSkin == 0 {
			r.clientSkin = 2
		}
		if r.clientControl == 0 {
			r.clientControl = 1
		}
		r.clientName = name

		p.room = r
		p.isHost = false
		p.name = name
		p.skin = r.clientSkin
		p.control = r.clientControl
		return nil
	default:
		return fmt.Errorf("invalid_role")
	}
}

func createRoom() *room {
	r := &room{
		code:          randomCode(),
		hostSkin:      1,
		hostControl:   2,
		clientSkin:    2,
		clientControl: 1,
		clientName:    "WAITING",
	}

	roomsMu.Lock()
	for {
		if _, exists := rooms[r.code]; !exists {
			rooms[r.code] = r
			break
		}
		r.code = randomCode()
	}
	roomsMu.Unlock()
	return r
}

func randomCode() string {
	const alphabet = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"
	var data [6]byte
	if _, err := rand.Read(data[:]); err != nil {
		return "AAAAAA"
	}
	for i := range data {
		data[i] = alphabet[int(data[i])%len(alphabet)]
	}
	return string(data[:])
}

func cleanupPeer(p *peer) {
	if p == nil || p.room == nil {
		return
	}

	p.closeOnce.Do(func() {
		_ = p.conn.Close()

		r := p.room
		r.mu.Lock()
		defer r.mu.Unlock()

		if p.isHost {
			client := r.client
			r.host = nil
			r.client = nil
			r.gameStarted = false

			roomsMu.Lock()
			delete(rooms, r.code)
			roomsMu.Unlock()

			if client != nil {
				sendPacket(client, packetInfo, []byte("message=HOST_DISCONNECTED"))
				sendPacket(client, packetEnd, []byte("reason=HOST_LEFT"))
				_ = client.conn.Close()
			}
			return
		}

		if r.client == p {
			r.client = nil
			r.clientName = "WAITING"
			r.clientSkin = 2
			r.clientControl = 1
			r.gameStarted = false
			if r.host != nil {
				sendPacket(r.host, packetInfo, []byte("message=CLIENT_DISCONNECTED"))
			}
			broadcastLobbyLocked(r, "WAITING_FOR_PLAYER")
		}
	})
}

func handleProfilePacket(p *peer, payload string) {
	if p == nil || p.room == nil {
		return
	}

	name := sanitizeToken(valueFor(payload, "name"), false)
	if name == "" {
		name = p.name
	}
	skin := clampInt(parseInt(valueFor(payload, "skin"), p.skin), 1, 2)
	control := clampInt(parseInt(valueFor(payload, "control"), p.control), 1, 2)

	r := p.room
	r.mu.Lock()
	defer r.mu.Unlock()

	p.name = name
	p.skin = skin
	p.control = control

	if p.isHost {
		r.hostName = name
		r.hostSkin = skin
		r.hostControl = control
		if r.client != nil && r.clientSkin == r.hostSkin {
			r.clientSkin = otherSkin(r.hostSkin)
			r.client.skin = r.clientSkin
		}
	} else {
		r.clientName = name
		r.clientSkin = skin
		r.clientControl = control
		if r.host != nil && r.hostSkin == r.clientSkin {
			r.hostSkin = otherSkin(r.clientSkin)
			r.host.skin = r.hostSkin
		}
	}

	broadcastLobbyLocked(r, "LOBBY_UPDATED")
}

func handleStartPacket(p *peer) {
	if p == nil || p.room == nil || !p.isHost {
		return
	}

	r := p.room
	r.mu.Lock()
	defer r.mu.Unlock()
	if r.client == nil {
		return
	}
	if r.hostSkin == r.clientSkin {
		r.clientSkin = otherSkin(r.hostSkin)
		if r.client != nil {
			r.client.skin = r.clientSkin
		}
	}

	r.gameStarted = true
	sendPacket(r.client, packetStart, nil)
	broadcastLobbyLocked(r, "GAME_STARTED")
}

func handleKickPacket(p *peer) {
	if p == nil || p.room == nil || !p.isHost {
		return
	}

	r := p.room
	r.mu.Lock()
	client := r.client
	r.client = nil
	r.clientName = "WAITING"
	r.clientSkin = 2
	r.clientControl = 1
	r.gameStarted = false
	broadcastLobbyLocked(r, "CLIENT_KICKED")
	r.mu.Unlock()

	if client != nil {
		sendPacket(client, packetKick, []byte("reason=HOST_KICKED"))
		_ = client.conn.Close()
	}
}

func handleInputPacket(p *peer, payload []byte) {
	if p == nil || p.room == nil || p.isHost {
		return
	}

	r := p.room
	r.mu.Lock()
	host := r.host
	r.mu.Unlock()
	if host != nil {
		sendPacket(host, packetInput, payload)
	}
}

func handleFramePacket(p *peer, payload []byte) {
	if p == nil || p.room == nil || !p.isHost {
		return
	}

	r := p.room
	r.mu.Lock()
	client := r.client
	r.mu.Unlock()
	if client != nil {
		sendPacket(client, packetFrame, payload)
	}
}

func handleSavePacket(p *peer, payload []byte) {
	if p == nil || p.room == nil || !p.isHost {
		return
	}

	r := p.room
	r.mu.Lock()
	client := r.client
	r.mu.Unlock()
	if client != nil {
		sendPacket(client, packetSave, payload)
	}
}

func handleEndPacket(p *peer, payload []byte) {
	if p == nil || p.room == nil || !p.isHost {
		return
	}

	r := p.room
	r.mu.Lock()
	r.gameStarted = false
	client := r.client
	broadcastLobbyLocked(r, "SESSION_ENDED")
	r.mu.Unlock()

	if client != nil {
		sendPacket(client, packetEnd, payload)
	}
}

func handlePausePacket(p *peer, payload []byte) {
	if p == nil || p.room == nil {
		return
	}

	r := p.room
	r.mu.Lock()
	var target *peer
	if p.isHost {
		target = r.client
	} else {
		target = r.host
	}
	r.mu.Unlock()

	if target != nil {
		sendPacket(target, packetPause, payload)
	}
}

func broadcastLobby(r *room, notice string) {
	if r == nil {
		return
	}
	r.mu.Lock()
	defer r.mu.Unlock()
	broadcastLobbyLocked(r, notice)
}

func broadcastLobbyLocked(r *room, notice string) {
	if r == nil {
		return
	}

	clientConnected := 0
	if r.client != nil {
		clientConnected = 1
	}
	payload := fmt.Sprintf(
		"code=%s host_name=%s host_skin=%d host_control=%d client_connected=%d client_name=%s client_skin=%d client_control=%d game_started=%d notice=%s",
		r.code,
		sanitizeToken(r.hostName, false),
		r.hostSkin,
		r.hostControl,
		clientConnected,
		sanitizeToken(r.clientName, false),
		r.clientSkin,
		r.clientControl,
		boolToInt(r.gameStarted),
		sanitizeToken(notice, true),
	)

	if r.host != nil {
		sendPacket(r.host, packetLobby, []byte(payload))
	}
	if r.client != nil {
		sendPacket(r.client, packetLobby, []byte(payload))
	}
}

func sendPacket(p *peer, packetType uint32, payload []byte) {
	if p == nil {
		return
	}
	writePacketWithLock(p.conn, &p.writeMu, packetType, payload)
}

func writePacket(conn net.Conn, packetType uint32, payload []byte) error {
	var header [8]byte
	binary.BigEndian.PutUint32(header[0:4], packetType)
	binary.BigEndian.PutUint32(header[4:8], uint32(len(payload)))
	if err := writeAll(conn, header[:]); err != nil {
		return err
	}
	if len(payload) == 0 {
		return nil
	}
	return writeAll(conn, payload)
}

func writePacketWithLock(conn net.Conn, mu *sync.Mutex, packetType uint32, payload []byte) {
	mu.Lock()
	defer mu.Unlock()
	if err := writePacket(conn, packetType, payload); err != nil {
		log.Printf("write packet %d: %v", packetType, err)
	}
}

func readPacket(conn net.Conn) (uint32, []byte, error) {
	var header [8]byte
	if _, err := io.ReadFull(conn, header[:]); err != nil {
		return 0, nil, err
	}

	packetType := binary.BigEndian.Uint32(header[0:4])
	payloadLen := binary.BigEndian.Uint32(header[4:8])
	if payloadLen > 32*1024*1024 {
		return 0, nil, fmt.Errorf("payload too large: %d", payloadLen)
	}

	payload := make([]byte, payloadLen)
	if payloadLen > 0 {
		if _, err := io.ReadFull(conn, payload); err != nil {
			return 0, nil, err
		}
	}
	return packetType, payload, nil
}

func writeAll(conn net.Conn, data []byte) error {
	for len(data) > 0 {
		n, err := conn.Write(data)
		if err != nil {
			return err
		}
		data = data[n:]
	}
	return nil
}

func valueFor(payload string, key string) string {
	fields := strings.Fields(payload)
	prefix := key + "="
	for _, field := range fields {
		if strings.HasPrefix(field, prefix) {
			return strings.TrimPrefix(field, prefix)
		}
	}
	return ""
}

func parseInt(text string, fallback int) int {
	if text == "" {
		return fallback
	}
	value, err := strconv.Atoi(text)
	if err != nil {
		return fallback
	}
	return value
}

func clampInt(value, minValue, maxValue int) int {
	if value < minValue {
		return minValue
	}
	if value > maxValue {
		return maxValue
	}
	return value
}

func otherSkin(skin int) int {
	if skin == 1 {
		return 2
	}
	return 1
}

func boolToInt(value bool) int {
	if value {
		return 1
	}
	return 0
}

func sanitizeToken(text string, upper bool) string {
	if text == "" {
		return ""
	}

	var b strings.Builder
	for _, r := range text {
		if (r >= 'a' && r <= 'z') || (r >= 'A' && r <= 'Z') || (r >= '0' && r <= '9') || r == '_' || r == '-' {
			if upper {
				b.WriteRune(rune(strings.ToUpper(string(r))[0]))
			} else {
				b.WriteRune(r)
			}
		}
	}
	return b.String()
}

func envDefault(key, fallback string) string {
	value := os.Getenv(key)
	if value == "" {
		return fallback
	}
	return value
}
