#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet-socket-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"
#include <iostream>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdhocWifiExperiment");

class Experiment {
public:
    Experiment();
    void Run(std::string rate, std::string manager);
    Vector GetPosition(uint32_t node) const;
    void SetPosition(uint32_t node, Vector position);
    void MoveNode(uint32_t node, Vector newPosition, Time duration);
    void ReceivePacket(Ptr<Socket> socket);
    void SetupPacketReception(Ptr<Node> node);

private:
    NodeContainer nodes;
    NetDeviceContainer devices;
    Ipv4InterfaceContainer interfaces;
    MobilityHelper mobility;
    PacketSocketHelper packetSocketHelper;
    std::map<uint32_t, Vector> nodePositions;
    std::vector<double> throughputValues;
    Time simulationTime;

    void GenerateGnuplot(std::string rate, std::string manager);
};

Experiment::Experiment() : simulationTime(Seconds(10)) {}

Vector Experiment::GetPosition(uint32_t node) const {
    auto it = nodePositions.find(node);
    if (it != nodePositions.end()) {
        return it->second;
    }
    return Vector(0, 0, 0);
}

void Experiment::SetPosition(uint32_t node, Vector position) {
    nodePositions[node] = position;
}

void Experiment::MoveNode(uint32_t node, Vector newPosition, Time duration) {
    mobility.GetPositionAllocator()->SetAttribute("Min", VectorValue(GetPosition(node)));
    mobility.GetPositionAllocator()->SetAttribute("Max", VectorValue(newPosition));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes.Get(node));
    Simulator::Schedule(duration, &Experiment::SetPosition, this, node, newPosition);
}

void Experiment::ReceivePacket(Ptr<Socket> socket) {
    Address from;
    Ptr<Packet> packet = socket->RecvFrom(from);
    if (packet) {
        Time arrival_time = Simulator::Now();
        throughputValues.push_back(packet->GetSize() * 8.0 / 1e6);
    }
}

void Experiment::SetupPacketReception(Ptr<Node> node) {
    TypeId tid = TypeId::LookupByName("ns3::PacketSocketFactory");
    Ptr<Socket> socket = Socket::CreateSocket(node, tid);
    packetSocketHelper.AddAddress(InetSocketAddress{Ipv4Address::GetAny(), 0});
    socket->Bind();
    socket->SetRecvCallback(MakeCallback(&Experiment::ReceivePacket, this));
}

void Experiment::Run(std::string rate, std::string manager) {
    nodes.Create(2);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    wifi.SetRemoteStationManager(manager, "DataMode", StringValue(rate), "RtsCtsThreshold", UintegerValue(2000));

    NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
    mac.SetType("ns3::AdhocWifiMac");

    devices = wifi.Install(phy, mac, nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    interfaces = address.Assign(devices);

    mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                  "X", StringValue("1.0"),
                                  "Y", StringValue("1.0"),
                                  "Rho", StringValue("ns3::UniformRandomVariable[Min=0|Max=5]"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    SetPosition(0, Vector(0.0, 0.0, 0.0));
    SetPosition(1, Vector(10.0, 10.0, 0.0));

    Ptr<Node> node0 = nodes.Get(0);
    Ptr<Node> node1 = nodes.Get(1);

    SetupPacketReception(node1);

    uint16_t port = 9;
    OnOffHelper onoff("ns3::PacketSocketFactory", Address(InetSocketAddress(interfaces.GetAddress(1), port)));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));
    onoff.SetAttribute("DataRate", StringValue("1Mbps"));

    ApplicationContainer app = onoff.Install(nodes.Get(0));
    app.Start(Seconds(1.0));
    app.Stop(simulationTime - Seconds(1.0));

    Simulator::Stop(simulationTime);

    Simulator::Run();

    Simulator::Destroy();

    GenerateGnuplot(rate, manager);
}

void Experiment::GenerateGnuplot(std::string rate, std::string manager) {
    std::string filenamePrefix = "wifi-adhoc";
    std::string dataFilename = filenamePrefix + "-" + rate + "-" + manager + ".dat";
    std::string graphicsFilename = filenamePrefix + "-" + rate + "-" + manager + ".png";
    std::string plotFilename = filenamePrefix + "-" + rate + "-" + manager + ".plt";

    std::ofstream dataFile(dataFilename.c_str());
    for (size_t i = 0; i < throughputValues.size(); ++i) {
        dataFile << i << " " << throughputValues[i] << std::endl;
    }
    dataFile.close();

    std::ofstream plotFile(plotFilename.c_str());
    plotFile << "set terminal png size 640,480" << std::endl;
    plotFile << "set output \"" << graphicsFilename << "\"" << std::endl;
    plotFile << "set title \"Adhoc WiFi Throughput (" << rate << ", " << manager << ")\"" << std::endl;
    plotFile << "set xlabel \"Time (s)\"" << std::endl;
    plotFile << "set ylabel \"Throughput (Mbps)\"" << std::endl;
    plotFile << "plot \"" << dataFilename << "\" using 1:2 with lines title \"Throughput\"" << std::endl;
    plotFile.close();

    std::string command = "gnuplot " + plotFilename;
    system(command.c_str());
}

int main(int argc, char* argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    std::vector<std::string> rates = {"DsssRate1Mbps", "DsssRate2Mbps", "DsssRate5_5Mbps", "DsssRate11Mbps"};
    std::vector<std::string> managers = {"ns3::AarfWifiManager", "ns3::AparfWifiManager", "ns3::AmrrWifiManager", "ns3::ArfWifiManager", "ns3::IdealWifiManager"};

    for (const auto& rate : rates) {
        for (const auto& manager : managers) {
            Experiment experiment;
            experiment.Run(rate, manager);
        }
    }

    return 0;
}