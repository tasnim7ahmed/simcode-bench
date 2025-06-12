#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/packet-socket-helper.h"
#include "ns3/applications-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

class Experiment
{
public:
    Experiment();
    void Configure(std::string phyMode, std::string rateManager, std::string outputFileName);
    void Run();

private:
    void SetPosition(Ptr<Node> node, Vector position);
    Vector GetPosition(Ptr<Node> node);
    void MoveNodes();
    void ReceivePacket(Ptr<Socket> socket);
    void SetupPacketReceive(Ptr<Node> node);
    void ThroughputSample();

    NodeContainer nodes;
    NetDeviceContainer devices;
    MobilityHelper mobility;
    Ptr<Socket> recvSocket;
    DataRate appDataRate;
    uint32_t bytesTotal;
    Gnuplot2dDataset dataset;
    std::string phyMode;
    std::string rateManager;
    std::string outputFileName;
};

Experiment::Experiment()
    : bytesTotal(0)
{
}

void Experiment::Configure(std::string mode, std::string manager, std::string fileName)
{
    phyMode = mode;
    rateManager = manager;
    outputFileName = fileName;
    appDataRate = DataRate("1Mbps");
}

void Experiment::SetPosition(Ptr<Node> node, Vector position)
{
    Ptr<MobilityModel> mob = node->GetObject<MobilityModel>();
    if (mob)
    {
        mob->SetPosition(position);
    }
}

Vector Experiment::GetPosition(Ptr<Node> node)
{
    Ptr<MobilityModel> mob = node->GetObject<MobilityModel>();
    if (mob)
    {
        return mob->GetPosition();
    }
    return Vector();
}

void Experiment::MoveNodes()
{
    Vector posA = GetPosition(nodes.Get(0));
    posA.y += 10.0;
    SetPosition(nodes.Get(0), posA);

    Vector posB = GetPosition(nodes.Get(1));
    posB.y -= 10.0;
    SetPosition(nodes.Get(1), posB);
}

void Experiment::ReceivePacket(Ptr<Socket> socket)
{
    Ptr<Packet> packet;
    while ((packet = socket->Recv()))
    {
        bytesTotal += packet->GetSize();
    }
}

void Experiment::SetupPacketReceive(Ptr<Node> node)
{
    TypeId tid = TypeId::LookupByName("ns3::PacketSocketFactory");
    recvSocket = Socket::CreateSocket(node, tid);
    PacketSocketAddress addr;
    addr.SetSingleDevice(devices.Get(1)->GetIfIndex());
    addr.SetPhysicalAddress(devices.Get(1)->GetAddress());
    addr.SetProtocol(0);
    recvSocket->Bind(addr);
    recvSocket->SetRecvCallback(MakeCallback(&Experiment::ReceivePacket, this));
}

void Experiment::ThroughputSample()
{
    double timeNow = Simulator::Now().GetSeconds();
    double throughput = bytesTotal * 8.0 / 1000000.0; // Mbps
    dataset.Add(timeNow, throughput);
    bytesTotal = 0;
    if (Simulator::Now().GetSeconds() < 10.0)
    {
        Simulator::Schedule(Seconds(1.0), &Experiment::ThroughputSample, this);
    }
}

void Experiment::Run()
{
    nodes.Create(2);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);
    wifi.SetRemoteStationManager(rateManager, "DataMode", StringValue(phyMode), "ControlMode", StringValue(phyMode));

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    devices = wifi.Install(phy, mac, nodes);

    PacketSocketHelper packetSocket;
    packetSocket.Install(nodes);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    SetPosition(nodes.Get(0), Vector(0.0, 0.0, 0.0));
    SetPosition(nodes.Get(1), Vector(5.0, 0.0, 0.0));

    SetupPacketReceive(nodes.Get(1));

    PacketSocketAddress socketAddr;
    socketAddr.SetSingleDevice(devices.Get(0)->GetIfIndex());
    socketAddr.SetPhysicalAddress(devices.Get(1)->GetAddress());
    socketAddr.SetProtocol(0);

    OnOffHelper onoff("ns3::PacketSocketFactory", Address(socketAddr));
    onoff.SetAttribute("DataRate", DataRateValue(appDataRate));
    onoff.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer apps = onoff.Install(nodes.Get(0));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    Simulator::Schedule(Seconds(3.0), &Experiment::MoveNodes, this);
    Simulator::Schedule(Seconds(1.0), &Experiment::ThroughputSample, this);

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    Gnuplot plot(outputFileName, "Adhoc WiFi Throughput vs. Time");
    dataset.SetTitle(phyMode + " - " + rateManager);
    dataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);
    plot.AddDataset(dataset);
    std::ofstream out(outputFileName);
    plot.GenerateOutput(out);
    out.close();
}

int main(int argc, char* argv[])
{
    CommandLine cmd;
    std::string outputPrefix = "wifi-adhoc";
    cmd.AddValue("outputPrefix", "Prefix for output gnuplot files", outputPrefix);
    cmd.Parse(argc, argv);

    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);

    std::vector<std::string> phyModes = {"ErpOfdmRate6Mbps", "ErpOfdmRate24Mbps", "ErpOfdmRate54Mbps"};
    std::vector<std::string> rateManagers = {"ns3::ConstantRateWifiManager", "ns3::AarfWifiManager"};

    for (const auto& mode : phyModes)
    {
        for (const auto& manager : rateManagers)
        {
            std::string fname = outputPrefix + "-" + mode + "-" + manager + ".plt";
            Experiment experiment;
            experiment.Configure(mode, manager, fname);
            experiment.Run();
        }
    }
    return 0;
}