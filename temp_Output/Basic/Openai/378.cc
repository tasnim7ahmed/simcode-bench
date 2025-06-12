#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mesh-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    NodeContainer nodes;
    nodes.Create(3);

    // Create the PHY and Channel
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    // Mesh helper with Random Access
    MeshHelper mesh = MeshHelper::Default();
    mesh.SetStackInstaller("ns3::Dot11sStack"); // mesh routing
    mesh.SetMacType("RandomStart", TimeValue(Seconds(0.1)), 
                    "MeshMacType", StringValue("RandomAccess"));
    mesh.SetNumberOfInterfaces(1);

    // Install mesh on nodes
    NetDeviceContainer meshDevices = mesh.Install(phy, nodes);

    // Assign IP addresses
    InternetStackHelper internetStack;
    internetStack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(meshDevices);

    // UDP Echo on Node 3 (server)
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(2)); // Node 3
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // UDP Echo Client on Node 1 (client, sends to Node 2 first)
    // We'll send to Node 2, then have Node 2 forward to Node 3

    // Forwarding application: simple UDP forwarder
    // First, setup sink on Node 2 to receive from Node 1
    uint16_t forwardPort = 7000;
    Address sinkAddress (InetSocketAddress (interfaces.GetAddress(1), forwardPort));
    PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory", sinkAddress);
    ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(1));
    sinkApps.Start(Seconds(1.0));
    sinkApps.Stop(Seconds(10.0));

    // Now, install UDP echo client on Node 1 (send to Node 2, port 7000)
    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), forwardPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(64));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Now, set up the forwarder on Node 2.
    // Custom app that receives UDP on port 7000 then sends UDP to node 3 (port 9).
    class UdpForwarder : public Application
    {
      public:
        UdpForwarder() {}
        void Setup(Address listenAddress, Address destAddress, uint16_t listenPort, uint16_t destPort)
        {
            m_listenAddress = listenAddress;
            m_destAddress = destAddress;
            m_listenPort = listenPort;
            m_destPort = destPort;
        }
      private:
        virtual void StartApplication()
        {
            m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
            InetSocketAddress local = InetSocketAddress(Ipv4Address::ConvertFrom(m_listenAddress), m_listenPort);
            m_socket->Bind(local);
            m_socket->SetRecvCallback(MakeCallback(&UdpForwarder::HandleRead, this));

            m_sendSocket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
        }
        virtual void StopApplication()
        {
            if (m_socket)
            {
                m_socket->Close();
                m_socket = 0;
            }
            if (m_sendSocket)
            {
                m_sendSocket->Close();
                m_sendSocket = 0;
            }
        }
        void HandleRead(Ptr<Socket> socket)
        {
            Ptr<Packet> packet;
            Address from;
            while ((packet = socket->RecvFrom(from)))
            {
                m_sendSocket->SendTo(packet, 0, InetSocketAddress(Ipv4Address::ConvertFrom(m_destAddress), m_destPort));
            }
        }
        Ptr<Socket> m_socket;
        Ptr<Socket> m_sendSocket;
        Address m_listenAddress;
        Address m_destAddress;
        uint16_t m_listenPort;
        uint16_t m_destPort;
    };

    Ptr<UdpForwarder> forwarder = CreateObject<UdpForwarder>();
    forwarder->Setup(interfaces.GetAddress(1), interfaces.GetAddress(2), forwardPort, port);
    nodes.Get(1)->AddApplication(forwarder);
    forwarder->SetStartTime(Seconds(1.0));
    forwarder->SetStopTime(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}