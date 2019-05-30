#include "VanGoghDeployer.h"

const int VALID_COMMAND = 0;
const char* SEPARATOR = "--------------------------------------------";
const char* FILE_NAME = "vangogh.tar.gz";
const int TRANSFER_SIZE = 1024;

constexpr char* BASE_CONFIG = 
"APP_NAME=VanGoghNationalPark\n"
"APP_ENV=local\n"
"APP_KEY=\n"
"APP_DEBUG=false\n"
"APP_URL={0}\n"
"\n"
"LOG_CHANNEL= stack\n"
"\n"
"DB_CONNECTION=mysql\n"
"DB_HOST={1}\n"
"DB_PORT={2}\n"
"DB_DATABASE={3}\n"
"DB_USERNAME={4}\n"
"DB_PASSWORD={5}\n"
"\n"
"BROADCAST_DRIVER=log\n"
"CACHE_DRIVER=file\n"
"QUEUE_CONNECTION=sync\n"
"SESSION_DRIVER=file\n"
"SESSION_LIFETIME=120\n";

std::string getInput(bool t_allow_empty = false)
{
	std::string input;

	while(input.size() == 0)
	{
		std::cin >> input;
		if (t_allow_empty) break;
	}

	return input;
}

std::string executeCommand(const ssh_session& t_session, const char* t_command) {
	ssh_channel channel = ssh_channel_new(t_session);
	if (channel == nullptr)
	{
		std::cout << "Er kon geen channel gemaakt worden op de remote host. Check uw server instellingen." << std::endl;
		ssh_disconnect(t_session);
		ssh_free(t_session);
		getchar();
		exit(-1);
	}

	auto response = ssh_channel_open_session(channel);
	if (response != SSH_OK)
	{
		std::cout << "Er kon geen channel geopend worden op de remote host. Check uw server instellingen." << std::endl;
		ssh_channel_free(channel);
		ssh_disconnect(t_session);
		ssh_free(t_session);
		getchar();
		exit(-1);
	}

	response = ssh_channel_request_exec(channel, t_command);
	if (response != SSH_OK)
	{
		std::cout << "Commando '" << t_command << "' kon niet worden uitgevoerd." << std::endl;
		ssh_channel_free(channel);
		ssh_disconnect(t_session);
		ssh_free(t_session);
		getchar();
		exit(-1);
	}

	char buffer[256];
	std::stringstream stringStream;
	auto nbytes = ssh_channel_read(channel, buffer, sizeof(buffer), 0);
	while (nbytes > 0)
	{
		stringStream.write(buffer, nbytes);
		nbytes = ssh_channel_read(channel, buffer, sizeof(buffer), 0);
	}

	ssh_channel_send_eof(channel);
	ssh_channel_close(channel);
	ssh_channel_free(channel);

	return stringStream.str();
}

void configure(std::string& t_config, int t_field, std::string t_value)
{
	size_t startPos = t_config.find("{" + std::to_string(t_field) + "}");
	if (startPos == std::string::npos)
	{
		return;
	}
	t_config.replace(startPos, 3, t_value);
}

bool transferFile(const ssh_session& t_session, std::string t_file, std::string t_remote)
{
	auto sftp = sftp_new(t_session);
	if (sftp == nullptr)
	{
		std::cout << "Kon geen SFTP starten op remote host." << std::endl;
		return false;
	}

	auto response = sftp_init(sftp);
	if (response != SSH_OK)
	{
		sftp_free(sftp);
		return false;
	}

	int accessType = O_RDWR | O_CREAT | O_TRUNC;
	sftp_file file = sftp_open(sftp, t_remote.c_str(), accessType, 0700);

	if(file == nullptr)
	{
		std::cout << "Kon file niet openen op remote host. Check bevoegdheden van folder." << std::endl;
		sftp_free(sftp);
		return false;
	}

	std::ifstream fileIn(t_file, std::ios::binary);
	fileIn.seekg(0, std::ios::end);
	fileIn.seekg(0);

	int count = 0;
	int filesize = fileIn.tellg();
	std::cout << "Aan het uploaden. Even geduld a.u.b.";
	while(true)
	{
		auto buffer = std::unique_ptr<char[]>(new char[TRANSFER_SIZE]);
		fileIn.read(buffer.get(), TRANSFER_SIZE);
		auto written = sftp_write(file, buffer.get(), TRANSFER_SIZE);

		if (written == -1)
		{
			std::cout << "Kon file niet wegschrijven op remote host." << std::endl;
			std::cout << ssh_get_error(t_session) << std::endl;
			sftp_free(sftp);
			return false;
		}

		if (fileIn.eof()) {
			break;
		}
	}
	std::cout << std::endl << "Upload succesvol." << std::endl;

	response = sftp_close(file);
	if (response != SSH_OK)
	{
		std::cout << "Kon file niet sluiten op remote host." << std::endl;
		sftp_free(sftp);
		return false;
	}

	return true;
}

bool commandFailed(std::string t_response)
{
	t_response.erase(std::remove(t_response.begin(), t_response.end(), '\n'), t_response.end());
	return atoi(t_response.c_str()) != VALID_COMMAND;
}

int main()
{
	std::cout << "Welkom bij de deployment tool voor de website van het Van Gogh National Park." << std::endl;
	std::cout << SEPARATOR << std::endl;
	std::cout << "Download de tar.gz door naar https://github.com/ItsBasvanDam/AgileL1/tarball/master te gaan." << std::endl;
	std::cout << "Hernoem deze naar 'vangogh.tar.gz'!" << std::endl;
	std::cout << SEPARATOR << std::endl;

	/*---------------------------------------------------------------------
	| Connectie
	*-------------------------------------------------------------------*/

	ssh_session sshSession = ssh_new();
	if (sshSession == nullptr)
	{
		std::cout << "SSL Session kon niet aangemaakt worden." << std::endl;
		exit(-1);
	}

	std::cout << "Geef het host adress op: (IP of Webadress, zonder poort)" << std::endl;
	auto host = getInput();

	ssh_options_set(sshSession, SSH_OPTIONS_HOST, host.c_str());

	std::cout << "Geef de SSH poort op van host '" << host << "':" << std::endl;
	auto port = atoi((getInput()).c_str());
	auto verbosity = SSH_LOG_NOLOG;

	ssh_options_set(sshSession, SSH_OPTIONS_LOG_VERBOSITY, &verbosity);
	ssh_options_set(sshSession, SSH_OPTIONS_PORT, &port);

	std::cout << "Connectie maken..." << std::endl;
	auto response = ssh_connect(sshSession);
	if(response != SSH_OK)
	{
		std::cout << "Er kon geen connectie gemaakt worden met host '" << host << "' op poort '" << port << "'. Check de gegevens van uw server en herstart het programma." << std::endl;
		ssh_free(sshSession);
		getchar();
		exit(-1);
	}

	std::cout << "Connectie succesvol." << std::endl;
	std::cout << SEPARATOR << std::endl;
	response = -1;
	while(response != SSH_AUTH_SUCCESS)
	{
		std::cout << "Geef uw gebruikersnaam op: " << std::endl;
		auto username = getInput();

		std::cout << "Geef uw wachtwoord op: " << std::endl;
		auto password = getInput();

		response = ssh_userauth_password(sshSession, username.c_str(), password.c_str());

		if(response == SSH_AUTH_DENIED || response == SSH_AUTH_ERROR)
		{
			std::cout << "Gebruikersnaam en/of wachtwoord incorrect, probeer opnieuw." << std::endl;
		}
	}

	std::cout << "Authenticatie succesvol" << std::endl;
	std::cout << SEPARATOR << std::endl;

	/*---------------------------------------------------------------------
	| Checks
	*-------------------------------------------------------------------*/

	std::cout << "Deployment begint." << std::endl;
	std::cout << "PHP check...";
	auto status = executeCommand(sshSession, "command -v php; echo $?");

	if(commandFailed(status))
	{
		std::cout << std::endl << "PHP is niet geinstalleerd, is niet gestart of de gebruiker heeft geen toegang." << std::endl;
		ssh_disconnect(sshSession);
		ssh_free(sshSession);
		getchar();
		exit(-1);
	}
	std::cout << "OK" << std::endl;

	std::cout << "MySQL check...";
	status = executeCommand(sshSession, "command -v mysql; echo $?");

	if (commandFailed(status))
	{
		std::cout << std::endl << "MySQL is niet geinstalleerd, is niet gestart of de gebruiker heeft geen toegang." << std::endl;
		ssh_disconnect(sshSession);
		ssh_free(sshSession);
		getchar();
		exit(-1);
	}
	std::cout << "OK" << std::endl;

	std::cout << "Composer check...";
	status = executeCommand(sshSession, "command -v composer; echo $?");

	if (commandFailed(status))
	{
		std::cout << std::endl << "Composer is niet geinstalleerd, is niet gestart of de gebruiker heeft geen toegang." << std::endl;
		ssh_disconnect(sshSession);
		ssh_free(sshSession);
		getchar();
		exit(-1);
	}
	std::cout << "OK" << std::endl;

	std::cout << SEPARATOR << std::endl;
	std::cout << "Geef het pad op waar '" << FILE_NAME << "' staat (eindig met /):" << std::endl;
	auto tarDir = getInput();

	while(!std::experimental::filesystem::exists(tarDir + FILE_NAME))
	{
		std::cout << "'" + tarDir + "vagngogh.tar.gz' niet gevonden, probeer opnieuw:" << std::endl;
		tarDir = getInput();
	}

	std::cout << "Geef het absolute pad op waar de website gedeployed moet worden (i.e. /var/www/html/, eindig met /):" << std::endl;
	auto dir = getInput();
	auto localFile = tarDir + FILE_NAME;
	auto remoteFile = dir + FILE_NAME;
	std::cout << SEPARATOR << std::endl;

	/*---------------------------------------------------------------------
	| Uploaden
	*-------------------------------------------------------------------*/

	std::cout << tarDir << " uploaden naar remote host locatie: ('" + remoteFile + "')..." << std::endl;
	if(!transferFile(sshSession, localFile, remoteFile))
	{
		std::cout << "Bestand kon niet geupload worden. Check de server en probeer opnieuw." << std::endl;
		ssh_disconnect(sshSession);
		ssh_free(sshSession);
		getchar();
		exit(-1);
	}

	std::cout << "Uitpakken..." << std::endl;

	auto command = "cd " + dir + "; tar -xzvf " + FILE_NAME + "; mv ItsBas* vangoghnationalpark; test -d vangoghnationalpark; echo $?";
	status = executeCommand(sshSession, command.c_str());

	if(commandFailed(status))
	{
		std::cout << "Kon .tar.gz niet uitpakken naar " << dir << "." << std::endl;
		ssh_disconnect(sshSession);
		ssh_free(sshSession);
		getchar();
		exit(-1);
	}

	std::cout << SEPARATOR << std::endl;

	/*---------------------------------------------------------------------
	| Configuratie
	*-------------------------------------------------------------------*/
	std::cout << "Configureer de website nu." << std::endl;
	auto config = std::string(BASE_CONFIG);

	std::cout << "Wat is de url van de website? (i.e. www.vangoghnationalpark.nl):" << std::endl;
	auto url = getInput();
	configure(config, APP_URL, url);

	std::cout << "Wat is de url van de mysql database? (127.0.0.1 voor localhost):" << std::endl;
	auto dbhost = getInput();
	configure(config, DB_HOST, dbhost);

	std::cout << "Wat is de poort van de mysql database? (standaard 3306):" << std::endl;
	auto dbport = getInput();
	configure(config, DB_PORT, dbport);

	std::cout << "Wat is de naam van de mysql database? (i.e. vangogh, deze moet al bestaan):" << std::endl;
	auto db = getInput();
	configure(config, DB_DATABASE, db);

	std::cout << "Wat is de username van de mysql database?" << std::endl;
	auto username = getInput();
	configure(config, DB_USERNAME, username);			

	std::cout << "Wat is het wachtwoord van de mysql database? (enter voor leeg)" << std::endl;
	auto password = getInput(true);
	configure(config, DB_PASSWORD, password);

	/*---------------------------------------------------------------------
	| Configuratie schrijven
	*-------------------------------------------------------------------*/
	std::cout << "Configuratie is nu compleet, opslaan..." << std::endl;
	CHAR documentFolder[MAX_PATH];
	HRESULT result = SHGetFolderPath(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, documentFolder);
	
	std::string envFile = std::string(documentFolder) + "\\.env";
	std::ofstream outfile(envFile);
	outfile << config << std::endl;
	outfile.close();

	transferFile(sshSession, envFile, dir + "vangoghnationalpark/.env");

	remove(envFile.c_str());

	/*---------------------------------------------------------------------
	| Deployment
	*-------------------------------------------------------------------*/
	std::cout << SEPARATOR << std::endl;

	std::cout << "Composer starten, even geduld a.u.b." << std::endl;
	command = "cd " + dir + "/vangoghnationalpark; composer install; echo $?";
	status = executeCommand(sshSession, command.c_str());

	if (commandFailed(status)) {
		std::cout << "Kon packages niet installeren via composer." << std::endl;
		ssh_disconnect(sshSession);
		ssh_free(sshSession);
		getchar();
		exit(-1);
	}
	std::cout << "OK." << std::endl;

	std::cout << "Database migreren, even geduld a.u.b." << std::endl;
	command = "cd " + dir + "/vangoghnationalpark; php artisan key:generate; php artisan migrate:fresh; php artisan db:seed; echo $?";
	status = executeCommand(sshSession, command.c_str());

	if (commandFailed(status)) {
		std::cout << "Kon database niet migreren via artisan." << std::endl;
		ssh_disconnect(sshSession);
		ssh_free(sshSession);
		getchar();
		exit(-1);
	}
	std::cout << "OK." << std::endl;
	std::cout << "De website is nu volledig opgezet in '" << dir << "vangoghnationalpark'. Hij kan gestart worden via 'php artisan serve' of via een andere service (Apache, Nginx, etc).";
	getchar();
	ssh_disconnect(sshSession);
	ssh_free(sshSession);
	return 0;
}
