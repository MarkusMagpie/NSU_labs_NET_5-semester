package snakegame.io;

import me.ippolitov.fit.snakes.SnakesProto;
import snakegame.io.datatypes.MessageWithSender;

import java.io.IOException;
import java.net.*;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;

public class MulticastManager {
    private static final int BUF_SIZE = 65000;

    private final BlockingQueue<SnakesProto.GameMessage> sendQueue = new LinkedBlockingQueue<>();
    private final BlockingQueue<MessageWithSender> receiveQueue = new LinkedBlockingQueue<>();

    private final MulticastSocket recvSocket;
    private final DatagramSocket sendSocket;

    private final Thread sendWorkerThread;
    private final Thread receiveWorkerThread;

    public MulticastManager(DatagramSocket sendSocket) throws IOException {
        this.sendSocket = sendSocket;
        this.recvSocket = new MulticastSocket(Config.MULTICAST_PORT);
        recvSocket.joinGroup(new InetSocketAddress(Config.MULTICAST_GROUP_IP, 0), null);

        sendWorkerThread = new Thread(this::sendWorker);
        sendWorkerThread.start();
        receiveWorkerThread = new Thread(this::receiveWorker);
        receiveWorkerThread.start();
    }

    void stop() {
        sendWorkerThread.interrupt();
        receiveWorkerThread.interrupt();
    }

    public void sendPacket(SnakesProto.GameMessage message) {
        sendQueue.add(message);
    }

    public MessageWithSender receivePacket() throws InterruptedException {
        return receiveQueue.take();
    }

    private void receiveWorker() {
        byte[] receiveBuffer = new byte[BUF_SIZE];
        while (true) {
            var receivePacket = new DatagramPacket(receiveBuffer, receiveBuffer.length);
            try {
                recvSocket.receive(receivePacket);
                byte[] bytes = new byte[receivePacket.getLength()];
                System.arraycopy(receiveBuffer, 0, bytes, 0, receivePacket.getLength());
                var gameMessage = SnakesProto.GameMessage.parseFrom(bytes);
                receiveQueue.add(
                        MessageWithSender.builder()
                                .ip(receivePacket.getAddress().getHostAddress())
                                .port(receivePacket.getPort())
                                .message(gameMessage)
                                .build()
                );
            } catch (IOException e) {}
        }
    }

    private void sendWorker() {
        while (true) {
            SnakesProto.GameMessage message;
            try {
                message = sendQueue.take();
            } catch (InterruptedException e) {
                break;
            }

            var sendData = message.toByteArray();

            try {
                var packet = new DatagramPacket(
                        sendData,
                        sendData.length,
                        InetAddress.getByName(Config.MULTICAST_GROUP_IP),
                        Config.MULTICAST_PORT
                );

                sendSocket.send(packet);
            } catch (IOException e) {}
        }
    }
}
