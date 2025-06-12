#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-new-reno.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarTopologyTcpDataset");

class TcpPacketLogger {
public:
    TcpPacketLogger(const std::string &filename);
    void LogPacket(Ptr<const Packet> packet, const Address &from, const Address &to, Time txTime, Time rxTime);

private:
    std::ofstream m_file;
};

TcpPacketLogger::TcpPacketLogger(const std::string &filename) {
    m_file.open(filename);
    m_file << "Source,Destination,Size,TransmissionTime,ReceptionTime\n";
}

void TcpPacketLogger::LogPacket(Ptr<const Packet> packet, const Address &from, const Address &, Time txTime, Time rxTime) {
    Ipv4Address fromAddr = InetSocketAddress::ConvertFrom(from).GetIpv4();
    uint32_t size = packet->GetSize();

    std::ostringstream srcStream;
    srcStream << fromAddr;
    std::string srcStr = srcStream.str();

    m_file << srcStr << "," << "10.0.0.0" << "," << size << "," << txTime.GetSeconds() << "," << rxTime.GetSeconds() << "\n";
}

static void AttachPacketLogger(Ptr<Node> node, TcpPacketLogger &logger) {
    node->GetDevice(0)->GetChannel()->TraceConnectWithoutContext("PktSnifferRx", MakeCallback(&TcpPacketLogger::LogPacket, &logger));
}

int main(int argc, char *argv[]) {
    std::string csvFilename = "tcp_dataset.csv";

    CommandLine cmd(__FILE__);
    cmd.AddValue("csvFile", "Name of CSV output file", csvFilename);
    cmd.Parse(argc, argv);

    // Enable TCP protocol
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));

    NodeContainer centralNode;
    centralNode.Create(1);

    NodeContainer peripheralNodes;
    peripheralNodes.Create(4);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("5ms"));

    NetDeviceContainer devices[4];
    for (int i = 0; i < 4; ++i) {
        devices[i] = p2p.Install(centralNode.Get(0), peripheralNodes.Get(i));
    }

    InternetStackHelper stack;
    stack.Install(centralNode);
    stack.Install(peripheralNodes);

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces[5];

    address.SetBase("10.0.0.0", "255.255.255.0");
    interfaces[0] = address.Assign(devices[0].Get(0)); // Central node interface 0
    interfaces[1] = address.Assign(devices[0].Get(1)); // Peripheral node 0

    for (int i = 1; i < 4; ++i) {
        address.NewNetwork();
        interfaces[0 + i + 1] = address.Assign(devices[i].Get(0)); // Central node interfaces 1-3
        address.NewNetwork();
        interfaces[1 + i + 3] = address.Assign(devices[i].Get(1)); // Peripheral nodes 1-3
    }

    uint16_t port = 5000;

    // Install server applications on the central node
    for (int i = 0; i < 4; ++i) {
        PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port + i));
        ApplicationContainer app = sink.Install(centralNode.Get(0));
        app.Start(Seconds(1.0));
        app.Stop(Seconds(10.0));
    }

    // Install client applications on each peripheral node
    for (int i = 0; i < 4; ++i) {
        OnOffHelper onoff("ns3::TcpSocketFactory", InetSocketAddress(interfaces[0].GetAddress(0), port + i));
        onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        onoff.SetAttribute("DataRate", StringValue("1Mbps"));
        onoff.SetAttribute("PacketSize", UintegerValue(1024));
        onoff.SetAttribute("MaxBytes", UintegerValue(102400));

        ApplicationContainer app = onoff.Install(peripheralNodes.Get(i));
        app.Start(Seconds(2.0 + i));
        app.Stop(Seconds(10.0));
    }

    TcpPacketLogger logger(csvFilename);

    // Attach packet logger to all interfaces of central node
    for (uint32_t i = 0; i < centralNode.Get(0)->GetNDevices(); ++i) {
        AttachPacketLogger(centralNode.Get(0), logger);
    }

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}