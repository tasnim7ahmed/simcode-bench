#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet-sink.h"
#include <fstream>

using namespace ns3;

std::ofstream cwndStream;

void CwndTracer(uint32_t oldCwnd, uint32_t newCwnd) {
    cwndStream << Simulator::Now().GetSeconds() << " " << newCwnd << std::endl;
}

void EnqueueTrace(std::string context, Ptr<const Packet> p) {
    static std::ofstream file("tcp-large-transfer.tr", std::ios::app);
    file << Simulator::Now().GetSeconds() << " " << context << ": Enqueue " << p->GetSize() << " bytes\n";
}

void DequeueTrace(std::string context, Ptr<const Packet> p) {
    static std::ofstream file("tcp-large-transfer.tr", std::ios::app);
    file << Simulator::Now().GetSeconds() << " " << context << ": Dequeue " << p->GetSize() << " bytes\n";
}

void ReceivePacketTrace(Ptr<const Packet> p, const Address &address) {
    static std::ofstream file("tcp-large-transfer.tr", std::ios::app);
    file << Simulator::Now().GetSeconds() << " Receive: " << p->GetSize() << " bytes\n";
}

class TcpSenderApp : public Application {
public:
    TcpSenderApp() : m_socket(0), m_peer(), m_totalBytes(0), m_sentBytes(0), m_sendSize(0) {}

    void Setup(Ptr<Socket> socket, Address address, uint32_t totalBytes, uint32_t sendSize) {
        m_socket = socket;
        m_peer = address;
        m_totalBytes = totalBytes;
        m_sendSize = sendSize;
    }

private:
    Ptr<Socket> m_socket;
    Address m_peer;
    uint32_t m_totalBytes;
    uint32_t m_sentBytes;
    uint32_t m_sendSize;

    virtual void StartApplication() override {
        m_socket->Bind();
        m_socket->Connect(m_peer);

        m_socket->SetConnectCallback(
            MakeCallback(&TcpSenderApp::ConnectionSucceeded, this),
            MakeNullCallback<void, Ptr<Socket> >()
        );
        m_socket->SetSendCallback(MakeCallback(&TcpSenderApp::DataSent, this));
    }

    virtual void StopApplication() override {
        if (m_socket) {
            m_socket->Close();
        }
    }

    void ConnectionSucceeded(Ptr<Socket> socket) {
        SendData();
    }

    void DataSent(Ptr<Socket> socket, uint32_t) {
        SendData();
    }

    void SendData() {
        while (m_sentBytes < m_totalBytes) {
            uint32_t left = m_totalBytes - m_sentBytes;
            uint32_t size = std::min(m_sendSize, left);
            Ptr<Packet> packet = Create<Packet>(size);
            int sent = m_socket->Send(packet);
            if (sent > 0) {
                m_sentBytes += sent;
            }
            if (sent < (int)size) {
                // Will retry on SendCallback
                break;
            }
        }
    }
};

int main(int argc, char *argv[]) {
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(3);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer d0 = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer d1 = p2p.Install(nodes.Get(1), nodes.Get(2));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer if0 = address.Assign(d0);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer if1 = address.Assign(d1);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Install PacketSink on n2
    uint16_t port = 50000;
    Address sinkAddress(InetSocketAddress(if1.GetAddress(1), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(2));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(20.0));

    // Create and install the TCP sender app on n0
    Ptr<Socket> srcSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());
    Ptr<TcpSenderApp> app = CreateObject<TcpSenderApp>();
    app->Setup(srcSocket, sinkAddress, 2000000, 1000); // Send 2MB, 1000-byte packets
    nodes.Get(0)->AddApplication(app);
    app->SetStartTime(Seconds(1.0));
    app->SetStopTime(Seconds(20.0));

    // Congestion window trace
    srcSocket->TraceConnectWithoutContext("CongestionWindow", MakeCallback(&CwndTracer));
    cwndStream.open("tcp-large-transfer.cwnd");

    // Trace queue events
    Config::Connect("/NodeList/0/DeviceList/0/$ns3::PointToPointNetDevice/TxQueue/Enqueue", MakeCallback(&EnqueueTrace));
    Config::Connect("/NodeList/0/DeviceList/0/$ns3::PointToPointNetDevice/TxQueue/Dequeue", MakeCallback(&DequeueTrace));
    Config::Connect("/NodeList/1/DeviceList/1/$ns3::PointToPointNetDevice/TxQueue/Enqueue", MakeCallback(&EnqueueTrace));
    Config::Connect("/NodeList/1/DeviceList/1/$ns3::PointToPointNetDevice/TxQueue/Dequeue", MakeCallback(&DequeueTrace));

    // Trace packet reception at sink
    Ptr<Application> sinkPtr = sinkApp.Get(0);
    Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkPtr);
    sink->TraceConnectWithoutContext("Rx", MakeCallback(&ReceivePacketTrace));

    // Enable PCAP tracing
    p2p.EnablePcap("tcp-large-transfer-0-0", d0.Get(0), true, true);
    p2p.EnablePcap("tcp-large-transfer-1-0", d0.Get(1), true, true);
    p2p.EnablePcap("tcp-large-transfer-1-1", d1.Get(0), true, true);
    p2p.EnablePcap("tcp-large-transfer-2-0", d1.Get(1), true, true);

    Simulator::Stop(Seconds(22.0));
    Simulator::Run();

    cwndStream.close();
    Simulator::Destroy();

    return 0;
}