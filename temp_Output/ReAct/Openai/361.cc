#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Configure point-to-point link
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Install TCP/IP stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Install TCP server (PacketSink) on node 1
    uint16_t serverPort = 8080;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), serverPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), serverPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(1));
    sinkApps.Start(Seconds(1.0));
    sinkApps.Stop(Seconds(10.0));

    // Install TCP client (OnOffApplication) on node 0
    OnOffHelper clientHelper("ns3::TcpSocketFactory", sinkAddress);
    clientHelper.SetAttribute("DataRate", StringValue("5Mbps"));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    clientHelper.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    clientHelper.SetAttribute("StopTime", TimeValue(Seconds(10.0)));

    // To achieve 0.1s packet interval, adjust "MaxBytes" and runtime so only one packet per 0.1s is sent.
    // However, OnOffApp is rate-driven, so instead:
    // Use BulkSendApplication with a socket, or a custom app.
    // Here use an Application to send a packet every 0.1s from 1s to 10s.
    Ptr<Socket> tcpSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());

    // Custom app to send packets every 0.1s
    class PeriodicTcpClient : public Application
    {
      public:
        PeriodicTcpClient() : m_socket(0), m_peer(), m_packetSize(1024), m_interval(Seconds(0.1))
        {}
        virtual ~PeriodicTcpClient()
        {
            m_socket = 0;
        }

        void Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, Time interval)
        {
            m_socket = socket;
            m_peer = address;
            m_packetSize = packetSize;
            m_interval = interval;
        }

      private:
        virtual void StartApplication()
        {
            m_socket->Connect(m_peer);
            SendPacket();
        }

        virtual void StopApplication()
        {
            if (m_sendEvent.IsRunning())
            {
                Simulator::Cancel(m_sendEvent);
            }
            if (m_socket)
            {
                m_socket->Close();
            }
        }

        void SendPacket()
        {
            Ptr<Packet> packet = Create<Packet>(m_packetSize);
            m_socket->Send(packet);
            m_sendEvent = Simulator::Schedule(m_interval, &PeriodicTcpClient::SendPacket, this);
        }

        Ptr<Socket> m_socket;
        Address m_peer;
        uint32_t m_packetSize;
        Time m_interval;
        EventId m_sendEvent;
    };

    Ptr<PeriodicTcpClient> app = CreateObject<PeriodicTcpClient>();
    app->Setup(tcpSocket, sinkAddress, 1024, Seconds(0.1));
    nodes.Get(0)->AddApplication(app);
    app->SetStartTime(Seconds(1.0));
    app->SetStopTime(Seconds(10.0));

    // Enable pcap tracing
    pointToPoint.EnablePcapAll("tcp-p2p-example");

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}