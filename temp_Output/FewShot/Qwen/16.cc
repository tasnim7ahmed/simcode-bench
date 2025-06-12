#include "ns3/core-module.h"
#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/log.h"
#include "ns3/gnuplot.h"
#include "ns3/mobility-module.h"
#include "ns3/packet-socket-client-server-helper.h"
#include "ns3/packet-socket-helper.h"
#include "ns3/wifi-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Experiment");

class Experiment {
public:
    Experiment();
    ~Experiment();

    void Run(uint32_t rate, std::string manager);
    Vector GetPosition(Ptr<Node> node);
    void SetPosition(Ptr<Node> node, Vector position);
    void ReceivePacket(Ptr<Socket> socket);
    void MoveNode(Ptr<Node> node, Vector newPosition);

private:
    Gnuplot2dDataset m_dataset;
    double m_throughput;
};

Experiment::Experiment() {
    m_throughput = 0.0;
    m_dataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);
}

Experiment::~Experiment() {}

Vector Experiment::GetPosition(Ptr<Node> node) {
    Ptr<MobilityModel> mobility = node->GetObject<MobilityModel>();
    return mobility->GetPosition();
}

void Experiment::SetPosition(Ptr<Node> node, Vector position) {
    Ptr<MobilityModel> mobility = node->GetObject<MobilityModel>();
    mobility->SetPosition(position);
}

void Experiment::MoveNode(Ptr<Node> node, Vector newPosition) {
    SetPosition(node, newPosition);
}

void Experiment::ReceivePacket(Ptr<Socket> socket) {
    Ptr<Packet> packet;
    while ((packet = socket->Recv())) {
        m_throughput += packet->GetSize();
    }
}

void Experiment::Run(uint32_t rate, std::string manager) {
    m_throughput = 0.0;

    NodeContainer nodes;
    nodes.Create(2);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::" + manager + "WifiManager");

    YansWifiPhyHelper phy;
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Assign constant mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    PacketSocketHelper packetSocket;
    packetSocket.Install(nodes);

    // Server application setup
    PacketSocketServerHelper server;
    server.SetLocal(InetSocketAddress(Ipv4Address::GetAny(), 80));
    ApplicationContainer serverApp = server.Install(nodes.Get(1));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(10.0));

    // Client application setup
    PacketSocketClientHelper client;
    client.SetRemote(InetSocketAddress(interfaces.GetAddress(1), 80));
    client.SetAttribute("MaxPackets", UintegerValue(1000000));
    client.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = client.Install(nodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    // Connect receive callback
    Config::Connect("/NodeList/1/ApplicationList/0/$ns3::PacketSocketServer/Rx",
                    MakeCallback(&Experiment::ReceivePacket, this));

    Simulator::Run();
    Simulator::Destroy();

    double duration = 9.0; // seconds of traffic (from 1 to 10)
    double throughput = (m_throughput * 8) / (duration * 1000000); // Mbps
    NS_LOG_INFO("Rate: " << rate << ", Manager: " << manager << ", Throughput: " << throughput << " Mbps");
    m_dataset.Add(rate, throughput);

    // Generate plot
    Gnuplot plot(manager + ".png");
    plot.SetTitle(manager + " WiFi Throughput vs Rate");
    plot.SetTerminal("png");
    plot.SetLegend("Data Rate (Mbps)", "Throughput (Mbps)");
    plot.AddDataset(m_dataset);
    std::ofstream plotFile(manager + ".plt");
    plot.GenerateOutput(plotFile);
    plotFile.close();
}

int main(int argc, char *argv[]) {
    uint32_t rate = 6;
    std::string manager = "Aarf";

    CommandLine cmd(__FILE__);
    cmd.AddValue("rate", "WiFi data rate in Mbps", rate);
    cmd.AddValue("manager", "WiFi remote station manager type", manager);
    cmd.Parse(argc, argv);

    Experiment experiment;
    experiment.Run(rate, manager);

    return 0;
}