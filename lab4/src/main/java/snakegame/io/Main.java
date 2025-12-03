package snakegame.io;

import me.ippolitov.fit.snakes.SnakesProto;

import java.io.IOException;
import java.net.DatagramSocket;
import java.net.SocketException;

public class Main {
    static void recv() {
        try {
            MulticastManager receiver = new MulticastManager(new DatagramSocket(5000));
            var msg = receiver.receivePacket();
        } catch (IOException | InterruptedException e) {
            e.printStackTrace();
        }
    }

    public static void testUnicast() throws SocketException {
        var sender = new UnicastManager(new DatagramSocket(5000));

        var messageBuilder = SnakesProto.GameMessage.newBuilder()
                .setPing(SnakesProto.GameMessage.PingMsg.getDefaultInstance());

        sender.sendPacket("localhost", 6000, messageBuilder.build());
    }

    public static void testMulticast() throws IOException {
        var sender = new MulticastManager(new DatagramSocket(5000));

        var message = SnakesProto.GameMessage.newBuilder()
                .setPing(SnakesProto.GameMessage.PingMsg.getDefaultInstance())
                .setMsgSeq(5)
                .build();

        new Thread(Main::recv).start();
        new Thread(Main::recv).start();
        new Thread(Main::recv).start();

        sender.sendPacket(message);
    }

    public static void killPlayer(PlayerController p) {
        p.stop();
    }
}
